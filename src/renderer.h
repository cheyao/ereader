#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>

#include "ports.h"

uint8_t glyph_pixel(const uint8_t* g, const int w, const int row,
					const int col);
void render_text(const char* text);

#endif // RENDERER_H
