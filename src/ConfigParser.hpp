#pragma once

// Copyright (c) 2017-2020 Matthias C. M. Troffaes - from inipp library
// Copyright (c) 2024 Yigithan Yigit
#include "FileSystem.hpp"

#include <cctype>
#include <list>
#include <string>

// Checkt is config starts with `;`
// If starts with `;` then it is a comment
// If not stars with but starts with `[` then it is a section
// Read the section name and read the `]` if not found then it is an error

class ConfigParser
{
  public:
    typedef std::map<std::string, std::string> Section;
    typedef std::map<std::string, Section> Config;

    Config config;
    std::list<std::string> errors;

    static inline const std::string char_section_start = "[";
    static inline const std::string char_section_end = "]";
    static inline const std::string char_assign = "=";
    static inline const std::string char_comment = ";";
    static inline const std::string char_interpol = "$";
    static inline const std::string char_interpol_start = "{";
    static inline const std::string char_interpol_sep = ":";
    static inline const std::string char_interpol_end = "}";

    ConfigParser(void) = default;

    void parse(const FileHandle file);
    void serialize(const FileHandle file);
};
