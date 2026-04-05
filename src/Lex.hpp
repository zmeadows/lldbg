#pragma once

#include <cctype>
#include <cstddef>
#include <string_view>
#include <unordered_set>
#include <vector>

enum class LexState
{
    Normal,
    InBlockComment
};

enum class TokenKind
{
    Normal,
    Keyword,
    String,
    Character,
    Comment,
    Number,
    Preprocessor,
    Identifier,
};

struct TokenSpan
{
    TokenKind kind;
    size_t start;
    size_t length;
};

namespace Lex
{
    inline bool is_identifier_start(char c)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        return std::isalpha(uc) || c == '_';
    }

    inline bool is_identifier_char(char c)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        return std::isalnum(uc) || c == '_';
    }

    inline bool is_digit(char c)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        return std::isdigit(uc);
    }

    inline bool is_whitespace(char c)
    {
        const unsigned char uc = static_cast<unsigned char>(c);
        return std::isspace(uc);
    }

    inline const std::unordered_set<std::string_view>& cpp_keywords()
    {
        static const std::unordered_set<std::string_view> keywords = {
            "alignas",
            "alignof",
            "asm",
            "auto",
            "bool",
            "break",
            "case",
            "catch",
            "char",
            "char8_t",
            "char16_t",
            "char32_t",
            "class",
            "concept",
            "const",
            "consteval",
            "constexpr",
            "constinit",
            "const_cast",
            "continue",
            "co_await",
            "co_return",
            "co_yield",
            "decltype",
            "default",
            "delete",
            "do",
            "double",
            "dynamic_cast",
            "else",
            "enum",
            "explicit",
            "export",
            "extern",
            "false",
            "float",
            "for",
            "friend",
            "goto",
            "if",
            "inline",
            "int",
            "long",
            "mutable",
            "namespace",
            "new",
            "noexcept",
            "nullptr",
            "operator",
            "private",
            "protected",
            "public",
            "register",
            "reinterpret_cast",
            "requires",
            "return",
            "short",
            "signed",
            "sizeof",
            "static",
            "static_assert",
            "static_cast",
            "struct",
            "switch",
            "template",
            "this",
            "thread_local",
            "throw",
            "true",
            "try",
            "typedef",
            "typeid",
            "typename",
            "union",
            "unsigned",
            "using",
            "virtual",
            "void",
            "volatile",
            "wchar_t",
            "while",
        };

        return keywords;
    }

    inline void push_token(std::vector<TokenSpan>& tokens, TokenKind kind, size_t start, size_t end)
    {
        if (end > start)
        {
            tokens.push_back(
                TokenSpan{
                    .kind = kind,
                    .start = start,
                    .length = end - start,
                }
            );
        }
    }

    inline size_t consume_string_like(std::string_view line, size_t i, char delimiter)
    {
        const size_t n = line.size();
        if (i >= n || line[i] != delimiter)
        {
            return i;
        }

        ++i; // consume opening quote

        while (i < n)
        {
            if (line[i] == '\\')
            {
                i += 2; // escaped char
                continue;
            }

            if (line[i] == delimiter)
            {
                ++i; // consume closing quote
                break;
            }

            ++i;
        }

        if (i > n)
        {
            i = n;
        }

        return i;
    }

    inline size_t consume_number(std::string_view line, size_t i)
    {
        const size_t n = line.size();
        const size_t start = i;

        if (i < n && line[i] == '0' && (i + 1) < n)
        {
            const char next = line[i + 1];
            if (next == 'x' || next == 'X')
            {
                i += 2;
                while (i < n &&
                       (std::isxdigit(static_cast<unsigned char>(line[i])) || line[i] == '\''))
                {
                    ++i;
                }
                while (i < n && std::isalpha(static_cast<unsigned char>(line[i])))
                {
                    ++i;
                }
                return i;
            }

            if (next == 'b' || next == 'B')
            {
                i += 2;
                while (i < n && (line[i] == '0' || line[i] == '1' || line[i] == '\''))
                {
                    ++i;
                }
                while (i < n && std::isalpha(static_cast<unsigned char>(line[i])))
                {
                    ++i;
                }
                return i;
            }
        }

        bool saw_dot = false;
        while (i < n)
        {
            const char c = line[i];
            if (is_digit(c) || c == '\'')
            {
                ++i;
            }
            else if (c == '.' && !saw_dot)
            {
                saw_dot = true;
                ++i;
            }
            else
            {
                break;
            }
        }

        if (i < n && (line[i] == 'e' || line[i] == 'E'))
        {
            ++i;
            if (i < n && (line[i] == '+' || line[i] == '-'))
            {
                ++i;
            }
            while (i < n && is_digit(line[i]))
            {
                ++i;
            }
        }

        while (i < n && std::isalpha(static_cast<unsigned char>(line[i])))
        {
            ++i;
        }

        return (i > start) ? i : start + 1;
    }

    inline std::vector<TokenSpan> lex_cpp_line(std::string_view line, LexState& state)
    {
        std::vector<TokenSpan> tokens;
        const size_t n = line.size();

        if (n == 0)
        {
            return tokens;
        }

        // Trim-left check for preprocessor directives
        {
            size_t first_non_ws = 0;
            while (first_non_ws < n && is_whitespace(line[first_non_ws]))
            {
                ++first_non_ws;
            }

            if (first_non_ws < n && line[first_non_ws] == '#')
            {
                tokens.push_back(
                    TokenSpan{
                        .kind = TokenKind::Preprocessor,
                        .start = 0,
                        .length = n,
                    }
                );
                return tokens;
            }
        }

        size_t i = 0;
        size_t normal_start = 0;

        const auto flush_normal_until = [&](size_t end, auto& self_tokens, auto& self_normal_start)
        {
            if (end > self_normal_start)
            {
                push_token(self_tokens, TokenKind::Normal, self_normal_start, end);
            }
        };

        while (i < n)
        {
            /*
             * Block comment
             * Test
             * 
             */
            if (state == LexState::InBlockComment)
            {
                size_t start = i;

                while ((i + 1) < n && !(line[i] == '*' && line[i + 1] == '/'))
                {
                    ++i;
                }

                if ((i + 1) < n)
                {
                    i += 2; // consume */
                    push_token(tokens, TokenKind::Comment, start, i);
                    state = LexState::Normal;
                    normal_start = i;
                    continue;
                }
                else
                {
                    // entire rest of line is still comment
                    push_token(tokens, TokenKind::Comment, start, n);
                    return tokens;
                }
            }
            else
            {
                // Line comment
                if ((i + 1) < n && line[i] == '/' && line[i + 1] == '/')
                {
                    flush_normal_until(i, tokens, normal_start);
                    push_token(tokens, TokenKind::Comment, i, n);
                    normal_start = n;
                    break;
                }

                // Block comment
                if ((i + 1) < n && line[i] == '/' && line[i + 1] == '*')
                {
                    flush_normal_until(i, tokens, normal_start);

                    const size_t start = i;
                    i += 2; // consume /*

                    while ((i + 1) < n && !(line[i] == '*' && line[i + 1] == '/'))
                    {
                        ++i;
                    }

                    if ((i + 1) < n)
                    {
                        i += 2; // consume */
                        push_token(tokens, TokenKind::Comment, start, i);
                        normal_start = i;
                    }
                    else
                    {
                        push_token(tokens, TokenKind::Comment, start, n);
                        state = LexState::InBlockComment;
                        return tokens;
                    }

                    continue;
                }

                // String literal
                if (line[i] == '"')
                {
                    flush_normal_until(i, tokens, normal_start);
                    const size_t end = consume_string_like(line, i, '"');
                    push_token(tokens, TokenKind::String, i, end);
                    i = end;
                    normal_start = i;
                    continue;
                }

                // Character literal
                if (line[i] == '\'')
                {
                    flush_normal_until(i, tokens, normal_start);
                    const size_t end = consume_string_like(line, i, '\'');
                    push_token(tokens, TokenKind::Character, i, end);
                    i = end;
                    normal_start = i;
                    continue;
                }

                // Number
                if (is_digit(line[i]))
                {
                    const bool previous_is_identifier = i > 0 && is_identifier_char(line[i - 1]);

                    if (!previous_is_identifier)
                    {
                        flush_normal_until(i, tokens, normal_start);
                        const size_t end = consume_number(line, i);
                        push_token(tokens, TokenKind::Number, i, end);
                        i = end;
                        normal_start = i;
                        continue;
                    }
                }

                // Identifier / keyword
                if (is_identifier_start(line[i]))
                {
                    const size_t start = i;
                    ++i;
                    while (i < n && is_identifier_char(line[i]))
                    {
                        ++i;
                    }

                    std::string_view ident = line.substr(start, i - start);
                    if (cpp_keywords().find(ident) != cpp_keywords().end())
                    {
                        flush_normal_until(start, tokens, normal_start);
                        push_token(tokens, TokenKind::Keyword, start, i);
                        normal_start = i;
                    }

                    continue;
                }

                ++i;
            }
        }

        if (normal_start < n)
        {
            push_token(tokens, TokenKind::Normal, normal_start, n);
        }

        return tokens;
    }
} // namespace Lex
