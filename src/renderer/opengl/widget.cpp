
#include <cstdio>
#include <cstring>
#include <string>
#include "pic/surface.h"
#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"
#include "renderer/opengl_widget.h"
#include "main.h"


#define MAX_CHARS 1024l


Graphics* TasBoxShader;


void gl_init_widget_box() {

    const char* vert = R"(
    #version 410 core
    layout (location = 0) in vec2 position;
    uniform vec4 Pos;
    out vec2 uv;
    out vec4 box;

    void main() {
        vec2 p = position * Pos.zw + Pos.xy;
        p = vec2(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0);
        gl_Position = vec4(p, 0.0, 1.0);

        uv = position;
        box = Pos;
    }
    )";

    const char* frag = R"(
    #version 410 core
    in vec2 uv;
    in vec4 box;
    out vec4 FragColor;
    uniform vec4 Color;

    void main() {
        FragColor = Color;
    }
    )";


    TasBoxShader = new Graphics("widget_box");
    TasBoxShader->set_vertex_shader(vert);
    TasBoxShader->set_fragment_shader(frag);
    TasBoxShader->add_input_floats(2, GL_FALSE);
    TasBoxShader->compile();

    float quad[] = { 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1 };
    TasBoxShader->buffer_data(6, quad, GL_STATIC_DRAW);
}



void _gl_text(color co, int x, int y, int size, const char* text);

void gl_init_tas_font();





void _draw_box(color co, int x, int y, int w, int h) {

    printf("draw box %i %i %i %i\n", x, y, w, h);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    TasBoxShader->use();
    TasBoxShader->uniform4f("Color", co.r/255.0f, co.g/255.0f, co.b/255.0f, co.a/255.0f);
    TasBoxShader->uniform4f("Pos",
        (float)x / SCREEN_WIDTH, (float)y / SCREEN_HEIGHT, (float)w / SCREEN_WIDTH, (float)h / SCREEN_HEIGHT
        );

    TasBoxShader->draw(0, 6);
}



int _gl_text_len(const char* text);






static void render(const UIWidget& box) {

    static bool initialized = false;

    if (!initialized) {
        initialized = true;
        gl_init_tas_font();
        gl_init_widget_box();
    }

    int fontSize = 18;
    auto font = get_font(fontSize);

    struct UIText { color co; int x; int y; const char* text; int textLen; };
    static std::vector<UIText> tasks;
    tasks.clear();
    static std::vector<std::tuple<short, const char*>> textbgs;
    textbgs.clear();

    color co = { 255, 255, 255, 255 };
    int cur_x = 0;
    int cur_y = 0;
    int cols = 0;
    auto s = box.text.c_str();
    auto end = s + box.text.size();


    auto cmd = [&](const char* pat) {
        if (*pat++ != *s) { return false; } // fast path
        auto ss = s+1;
        while (*pat) { if (*pat++ != *ss++) { return false; } }
        s = ss;
        if (s[0] == ' ') { s++; } // Skip single space after command
        return true;
    };




    while (s != end) {

        if (*s == ' ' || *s == '\n') {

            s++;

        } else if (cmd("P")) { // Print

            int x = font.char_width * cur_x;
            int y = font.line_height() * cur_y;
            auto cc = (char*)s;
            auto len = _gl_text_len(cc);
            tasks.push_back({ co, x, y, cc, len });
            cur_x += len;
            s += std::strlen(cc) + 1;
            cols = std::max(cur_x, cols);

        } else if (cmd("S")) { // Font size
            
            size_t n_chars;
            fontSize = std::stoi((const char*)s, &n_chars);
            font = get_font(fontSize);
            s += n_chars;

        } else if (cmd("R")) { // Return

            cur_x = 0;
            cur_y++;

        } else if (cmd("C")) { // Color

            co.r = *s++;
            co.b = *s++;
            co.g = *s++;
            co.a = *s++;

        } else if (cmd("BG")) {

            textbgs.emplace_back((short)tasks.size(), (const char*)s);
            s += 6; // Skip data

        } else if (cmd("E")) {

            break;

        } else {
            printf("cmd: %s\n", s);
            internal_error("Unknown render command");
        }
    }



    /*
     * Layout and draw box
     */

    int w = cols * font.char_width + box.paddingX * 2;
    int h = cur_y * font.line_height() + box.paddingY * 2;

    int x = box.x;
    if (box.x < 0) {
        x = SCREEN_WIDTH - w + box.x;
    } else if (box.x == 0xffee) { // center
        x = (SCREEN_WIDTH - w) / 2;
    }
    int y = box.y;
    if (box.y < 0) {
        y = SCREEN_HEIGHT - h + box.y;
    } else if (box.y == 0xffee) { // center
        y = (SCREEN_HEIGHT - w) / 2;
    }

    int content_x = x + box.paddingX;
    int content_y = y + box.paddingY;

    if (box.co.a > 0.0) {
        _draw_box(box.co, x, y, w, h);
    }





    /*
     * Render text bgs
     */

    for (auto [ off, s ] : textbgs) {

        int len = *s++ - '0';
        s++; // skip space
        auto co = color::from_str(s);

        int xmin = 1<<20;
        int ymin = 1<<20;
        int xmax = 0;
        int ymax = 0;

        for (auto i=off; i<off+len; i++) {
            auto& text = tasks[i];
            xmin = std::min(xmin, text.x + content_x);
            ymin = std::min(ymin, text.y + content_y);
            xmax = std::max(xmax, text.x + content_x + font.char_width * text.textLen);
            ymax = std::max(ymax, text.y + content_y + font.line_height());
        }

        _draw_box(co, xmin, ymin, xmax-xmin, ymax-ymin);
    }

 


    /*
     * Render texts
     */

    for (auto& text : tasks) {
        text.x += content_x;
        text.y += content_y;
        _gl_text(text.co, text.x, text.y, fontSize, text.text);
    }
}





void gl_render_widget(const UIWidget& box) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    render(box);
    glDisable(GL_BLEND);
}
