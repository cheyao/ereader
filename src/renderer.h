#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>

#include "ports.h"

extern uint32_t render_advance;

void prepare_text(const char* text);
void render_text(const char* text);
void render_text_partial(const char* text, const uint8_t buffer);

#endif // RENDERER_H
