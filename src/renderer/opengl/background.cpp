
#include "SDL_video.h"
#include "game/driver.h"
#include "game/game.h"
#include "physics/init.h"
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

        BG->set_vertex_shader(R"(
        #version 410 core

        layout(std140) uniform GlobalData {
            vec4 uFrustum;
            float PixelsToMeters;
            float time;
            uint mins;
            uint secs;
            uint csecs;
        };
        layout(location = 0) in float loc;
        uniform vec2 texSize;

        out vec2 uv;

        const vec2 verts[6] = vec2[](
            vec2(0,0), vec2(1,0), vec2(1,1),
            vec2(0,0), vec2(1,1), vec2(0,1)
        );

        void main()
        {
            vec2 v = verts[gl_VertexID];
            vec2 pos = uFrustum.xy * v + uFrustum.zw * (vec2(1.0)-v);

            uv = pos / (texSize * PixelsToMeters);

            vec2 p = (pos - uFrustum.xy) / (uFrustum.zw - uFrustum.xy);
            gl_Position = vec4(-1.0 + p.x * 2.0, -1.0 + p.y * 2.0, 0.0, 1.0);
        }
        )");

        auto vert = std::string(R"(
        #version 410 core
        layout(std140) uniform Palette { vec4 palette[256]; };
        layout(std140) uniform GlobalData {
            vec4 uFrustum;
            float PixelsToMeters;
            float time;
            uint mins;
            uint secs;
            uint csecs;
        };

        in vec2 uv;
        uniform usampler2D tex;
        out vec4 FragColor;

        )") + TimerGLSL + R"(

        void main() {
            uint index = texture(tex, uv).r;
            FragColor = palette[index];

            drawTimer();
        }
        )";

        BG->set_fragment_shader(vert.c_str());

        BG->compile();
        BG->bind_uniform_block(0, "Palette");
        BG->bind_uniform_block(1, "GlobalData");

        float dummy = 0.0;
        BG->buffer_data(1, &dummy, GL_STATIC_DRAW);
    }
}



GlLifecycle<> Background = {
    .init = &init,
    .on_lgr = []() {
        auto pic = LgrTexture.get_texture(-1);
        BG->set_texture(0, "tex", pic.tex);
        BG->uniform2f("texSize", pic.obj->pic->get_width(), pic.obj->pic->get_height());
    },
    .render = [] {
        BG->draw_instanced(0, 6, 1);
    }
};
