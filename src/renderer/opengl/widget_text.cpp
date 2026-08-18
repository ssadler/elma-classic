
#include "pic/surface.h"
#include "renderer/opengl_gfx.h"
#include "renderer/opengl_widget.h"
#include "renderer/render.h"
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <cstdint>

#include <regex>



#define MAX_CHARS 1024l



extern uint16_t ATLAS_COLS;
extern uint16_t ATLAS_ROWS;


extern std::vector<std::tuple<uint32_t, uint32_t>> ranges;



bool get_uv(uint32_t c, float &u0, float &v0, float &w, float &h) {

  if (c == ' ') {
    return false;
  }

  int off = 0;

  for (auto [a, b] : ranges) {

    if (c >= a && c <= b) {

      auto n = off + c - a;
      int x = n % ATLAS_COLS;
      int y = n / ATLAS_COLS;
      w = 1.0f / ATLAS_COLS;
      h = 1.0f / ATLAS_ROWS;
      u0 = w * x;
      v0 = h * y;
      return true;
    }

    off += b - a + 1;
  }

  return false;
}







gl_ring_buffer* TextBuf = nullptr;

Graphics* TasTextShader;

struct TasChar {
  float f[8];
  color shade;
  int flags;
};

static TasChar TasText[MAX_CHARS];





void gl_init_tas_font() {


  const char* vert = R"(
  #version 410 core
  layout(location=0) in vec2 xy;
  layout(location=1) in vec2 size;
  layout(location=2) in vec2 texUV;
  layout(location=3) in vec2 texSize;
  layout(location=4) in vec4 _color;
  layout(location=5) in int _bold;
  out vec4 color;
  flat out int bold;
  out vec2 vUV;


  const vec2 verts[6] = vec2[](
    vec2(0,0), vec2(1,0), vec2(1,1),
    vec2(0,0), vec2(1,1), vec2(0,1)
  );

  void main() {

    vec2 v = verts[gl_VertexID];
    vec2 pos = xy + size * v;
    gl_Position = vec4(pos, 0.0, 1.0);

    vUV = texUV + texSize * v;
    color = _color;
    bold = _bold;
  }
  )";

  const char* frag = R"(
  #version 410 core
  in vec2 vUV;
  in vec4 color;
  flat in int bold;
  out vec4 FragColor;
  
  uniform sampler2D fontNormal;
  uniform sampler2D fontBold;
  
  void main() {
    float a = bold > 0 ? texture(fontBold, vUV).r : texture(fontNormal, vUV).r;
    FragColor = vec4(color.rgb, color.a * a);
  }
  )";


  TasTextShader = new Graphics("widget_text");
  TasTextShader->set_vertex_shader(vert);
  TasTextShader->set_fragment_shader(frag);
  TasTextShader->add_input_floats(2, GL_FALSE); // xy
  TasTextShader->add_input_floats(2, GL_FALSE); // size
  TasTextShader->add_input_floats(2, GL_FALSE); // texUV
  TasTextShader->add_input_floats(2, GL_FALSE); // texSize
  TasTextShader->add_input_color();
  TasTextShader->add_input_ints(1);
  TasTextShader->vertex_array_binding_divisor = 1;
  TasTextShader->compile();

  //TasTextShader->enable_ring(MAX_CHARS);
}




std::array<float, 4> textPos(font& f, int x, int y, int len) {

  y += f.line_offset(0);

  if (x < 0) {
    int w = f.char_width * len;
    x = SCREEN_WIDTH - w + x;
  }

  return {
    -1.0f + x / float(SCREEN_WIDTH) * 2.0f,
    1.0f  - y / float(SCREEN_HEIGHT) * 2.0f,
    f.char_width * 2.0f / SCREEN_WIDTH,
    -f.char_height * 2.0f / SCREEN_HEIGHT
  };
}



uint32_t next_utf8(const char*& sv) {
    unsigned char c = *sv++;

    if (c < 0x80) {
        return c;
    }

    if ((c >> 5) == 0b110) {
        uint32_t cp = (c & 0x1F) << 6;
        cp |= (*sv++ & 0x3F);
        return cp;
    }

    if ((c >> 4) == 0b1110) {
        uint32_t cp = (c & 0x0F) << 12;
        cp |= (*sv++ & 0x3F) << 6;
        cp |= (*sv++ & 0x3F);
        return cp;
    }

    if ((c >> 3) == 0b11110) {
        uint32_t cp = (c & 0x07) << 18;
        cp |= (*sv++ & 0x3F) << 12;
        cp |= (*sv++ & 0x3F) << 6;
        cp |= (*sv++ & 0x3F);
        return cp;
    }

    throw std::runtime_error("invalid UTF-8");
}



void _gl_text(color co, int x, int y, int size, const char* text) {

  size_t off = 0;
  int char_off = 0;

  bool bold = false;

  auto end = text + strlen(text);

  for (auto t = text; t < end;) {

    if (*t == '!' && *(t+1) == '[') {
      if (t + 3 < end && *(t+3) == ']') {
        bold = *(t+2) == 'B';
        t += 4;
      }
      if (t + 6 < end && *(t+6) == ']') {
        t += 2;
        co = color::from_str(t);
        t += 1;
      }
      continue;
    }

    float u, v, w, h;
    if (get_uv(next_utf8(t), u, v, w, h)) {
        //printf("%i %f %f %f %f %x %i\n", char_off, u, v, w, h, co, bold);
        TasText[off++] = { { float(char_off), 0, 0, 0, u, v, w, h }, co, bold };
    }

    char_off++;
  }

  auto font = get_font(size);

  auto pos = textPos(font, x, y, off);
  for (int i=0; i<off; i++) {
    auto off = TasText[i].f[0];
    memcpy(&TasText[i], &pos, 16);
    TasText[i].f[0] += pos[2] * off;
  }

  TasTextShader->use();
  TasTextShader->uniform1i("fontNormal", 0);
  TasTextShader->uniform1i("fontBold", 1);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, font.texture);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, get_font(size, true).texture);

  TasTextShader->buffer_data(off, TasText, GL_STREAM_DRAW);
  TasTextShader->draw_instanced(0, 6, off);
}



std::regex re_ctrl(R"(\!\[[^\]]*\])");

int _gl_text_len(const char* text) {
  std::match_results<std::string_view::const_iterator> match;

  const char* end = std::strchr(text, 0);
  int len = end - text;


  while (std::regex_search(text, end, match, re_ctrl)) {
    len -= match.length();
    text = match[0].second;
  }
  return len;
}

