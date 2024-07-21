#include "FileSystem.hpp"
#include "ConfigParser.hpp"
#include "test.h"

int main()
{
    auto open_files = OpenFiles();
    open_files.open("example_config/config.ini");
    auto handle = open_files.focus();
    auto config_parser = ConfigParser();
    if (!handle.has_value())
        std::cout << "File Not Found";
    config_parser.parse(*handle);

    ASSERT_MESSAGE(config_parser.errors.size() == 0, "Error parsing config file");

    auto first_section = config_parser.config.begin()->first;
    ASSERT_MESSAGE(first_section == "Example_Section", "First section is not section1");

    auto first_section_var1 = config_parser.config.begin()->second.begin()->first;
    auto first_section_var1_val = config_parser.config.begin()->second.begin()->second;
    ASSERT_MESSAGE(first_section_var1 == "var", "First section first variable is not var1");
    ASSERT_MESSAGE(first_section_var1_val == "3", "First section first variable value is not value1");
    auto first_section_var2 = next(config_parser.config.begin()->second.begin())->first;
    auto first_section_var2_val = next(config_parser.config.begin()->second.begin())->second;
    ASSERT_MESSAGE(first_section_var2 == "var1", "First section second variable is not var2");
    ASSERT_MESSAGE(first_section_var2_val == "Example string", "First section second variable value is not value2");

    auto second_section = (++config_parser.config.begin())->first;
    ASSERT_MESSAGE(second_section == "Example_Section2", "Second section is not section2");

    auto second_section_var1 = (++config_parser.config.begin())->second.begin()->first;
    auto second_section_var1_val = (++config_parser.config.begin())->second.begin()->second;
    ASSERT_MESSAGE(second_section_var1 == "var2", "Second section first variable is not var1");
    ASSERT_MESSAGE(second_section_var1_val == "6", "Second section first variable value is not value1");

    for (auto e : config_parser.config)
    {
        std::cout << e.first << std::endl;
        for (auto c : e.second)
        {
             std::cout << c.first << "=" << c.second << std::endl;
        }
    }
}
