
/*
 * Background is misnomer its fg
 */

#include "pic/lgr.h"
#include "pic/surface.h"
#include "renderer/canvas.h"
#include "renderer/render.h"
#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"
#include <cstring>




static Graphics* BG = nullptr;

static void init() {
    if (BG == nullptr) {
        BG = new Graphics("bg");
        BG->add_input_floats(1, false);
        BG->vertex_array_binding_divisor = 1;

        auto vert = SHADER_GLOBALS + R"(
        layout(location = 0) in float loc;
        uniform vec2 texSize;

        out vec2 uv;

        const vec2 verts[6] = vec2[](
            vec2(0,0), vec2(1,0), vec2(1,1),
            vec2(0,0), vec2(1,1), vec2(0,1)
        );

        void main() {
            vec2 v = verts[gl_VertexID];
            gl_Position = vec4(-1.0 + v.x * 2.0, -1.0 + v.y * 2.0, 0.0, 1.0);
            uv = (v * vec2(globals.screenSize) + vec2(globals.canvas_corner)) / texSize;
        }
        )";

        auto frag = SHADER_GLOBALS + R"(
        layout(std140) uniform Palette { vec4 palette[256]; };

        in vec2 uv;
        uniform usampler2D tex;
        out vec4 FragColor;

        )" + TimerGLSL + R"(

        void main() {
            uint index = texture(tex, uv).r;
            FragColor = palette[index];

            drawTimer();
        }
        )";

        BG->set_vertex_shader(vert.c_str());
        BG->set_fragment_shader(frag.c_str());

        BG->compile();
        BG->bind_uniform_block(0, "Palette");
        BG->bind_uniform_block(1, "GlobalData");

        float dummy = 0.0;
        BG->buffer_data(1, &dummy, GL_STATIC_DRAW);
    }
}



gl_lifecycle<> Background = {
    .init = &init,
    .on_lgr = []() {
        auto pic = LgrTexture.get_texture(-1);
        BG->set_texture(0, "tex", pic.tex);
        BG->uniform2f("texSize", pic.obj->pic->get_width(), pic.obj->pic->get_height());
    },
    .render = []() {
        BG->draw_instanced(0, 6, 1);
    }
};
