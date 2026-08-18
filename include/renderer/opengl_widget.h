#ifndef RENDER_OPENGL_WIDGET_H
#define RENDER_OPENGL_WIDGET_H

#include <cctype>
#include <cstdint>
#include <string>
#include "opengl.h"


/*
 * Text
 */

struct font {
  GLuint texture;
  int char_width;
  int char_height;
  float _line_height = 1.2;
  int line_height() const {
    return char_height * _line_height;
  }
  int line_offset(int n) const {
    return line_height() * n + (line_height() - char_height) / 2.0;
  }
};
font& get_font(int size);
font& get_font(int size, bool bold);



struct color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    static color from_floats(float* f) {
        color o;
        o.r = f[0] * 255;
        o.g = f[1] * 255;
        o.b = f[2] * 255;
        o.a = f[3] * 255;
        return o;
    }
    static color from_str(const char*& t) {
        color co;
        for (int i=0; i<4; i++) {
            unsigned char c = tolower(*t++);
            if (c >= '0' && c <= '9') {
                c -= '0';
            } else if (c >= 'a' && c <= 'f') {
                c = (c-'a') + 10;
            }
            co[i] = (c<<4) + c;
        }
        return co;
    }
    // Subscript operator
    uint8_t& operator[](std::size_t i) {
        return ((uint8_t*)this)[i];
    }
};
static_assert(sizeof(color) == 4);


struct UIWidget {
    color co{};
    int x{}; int y{};
    int paddingX{}; int paddingY{};
    std::string text;
    int font_px = 18;

    UIWidget(int x, int y, const std::string& text) {
        this->x = x;
        this->y = y;
        this->text = text;
    }
    UIWidget padding(int padding) {
        paddingX = padding;
        paddingY = padding;
        return *this;
    }
    UIWidget pos(int x, int y) {
        paddingX = x;
        paddingY = y;
        return *this;
    }
    UIWidget text_color(color col) {
        co = col;
        return *this;
    }
    UIWidget font_size(int size_px) {
        font_px = size_px;
        return *this;
    }
};


void gl_render_widget(const UIWidget& box);
void gl_clear_widget(const int id);



#endif
