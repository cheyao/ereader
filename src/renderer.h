#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>

#include "ports.h"

extern uint32_t render_advance;

void prepare_text(const char* text);
// Progress bar for the next render: done / total of the width (total 0 = off)
void set_progress(uint32_t done, uint32_t total);
void render_text(const char* text);
void render_prepared(const char* text);
void render_text_partial(const char* text, const uint8_t buffer);

#endif // RENDERER_H
