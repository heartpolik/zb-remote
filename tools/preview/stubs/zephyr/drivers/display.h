#pragma once
struct display_buffer_descriptor { int buf_size, width, height, pitch; };
#define display_write(...) 0
#define display_blanking_off(d) 0
#define display_blanking_on(d) 0
#define display_set_contrast(d,c) 0
