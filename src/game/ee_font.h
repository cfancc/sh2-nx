#pragma once
#include <stdint.h>

void ee_font_init(void);
void ee_font_overlay(uint32_t texture, uint32_t bits, uint32_t pitch, uint32_t width, uint32_t height);
