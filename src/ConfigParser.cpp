#include "ConfigParser.hpp"

#include "FileSystem.hpp"
#include "fmt/format.h"

#include <algorithm>
#include <string>
#include <vector>

static inline void ltrim(std::string& content)
{
    content.erase(
        content.begin(), std::find_if(
                             content.begin(), content.end(),
                             [](char ch)
                             {
                                 return !std::isspace(ch) && ch != '"';
                                 ;
                             }
                         )
    );
}

static inline void rtrim(std::string& content)
{
    content.erase(
        std::find_if(
            content.rbegin(), content.rend(),
            [](char ch)
            {
                return !std::isspace(ch) && ch != '"';
                ;
            }
        ).base(),
        content.end()
    );
}

static inline void trim(std::string& content)
{
    ltrim(content);
    rtrim(content);
}

void ConfigParser::parse(FileHandle file)
{
    auto contents = file.contents();
    std::string section;
    for (auto line : contents)
    {
        if (line.empty())
            continue;

        trim(line);
        const auto assign_pos = line.find_first_of(char_assign);
        const auto& front = line.front();
        if (front == char_comment.front())
            continue;
        else if (front == char_section_start.front())
        {
            if (line.back() == char_section_end.front())
                section = line.substr(1, prev(line.end()) - line.begin() - 1);
            else
                errors.emplace_back(fmt::format("Error!, No section close in ({})", line.c_str()));
        }
        else if (assign_pos != std::string::npos && assign_pos != 0)
        {
            std::string var(line.substr(0, assign_pos));
            std::string value(line.substr(assign_pos + 1, line.length()));
            trim(var);
            trim(value);
            auto& sec = config[section];
            if (sec.find(var) == sec.end())
                sec.insert(std::make_pair(var, value));
            else
                errors.emplace_back(
                    fmt::format("Error!, Variable Redeclaration found in ({})", line.c_str())
                );
        }
    }
}
