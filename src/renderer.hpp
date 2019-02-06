#pragma once

namespace lldbg {

void initialize_renderer(int argc, char** argv);
void destroy_renderer(void);
void glut_display_func();
void my_display_code();

}
