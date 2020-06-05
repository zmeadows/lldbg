#include "FileViewer.hpp"

#include "StringBuffer.hpp"
#include "imgui.h"

void FileViewer::render(void)
{
    StringBuffer line_buffer;
    for (size_t i = 0; i < m_lines.size(); i++) {
        const size_t line_number = i + 1;
        if (m_breakpoints.find(line_number) != m_breakpoints.end()) {
            line_buffer.format(" X {}  {}\n", line_number, m_lines[i]);
        }
        else {
            line_buffer.format("   {}  {}\n", line_number, m_lines[i]);
        }
        ImGui::TextUnformatted(line_buffer.data());
        line_buffer.clear();
    }
}
