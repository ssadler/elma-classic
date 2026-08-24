
#include "pic/lgr.h"
#include "pic/surface.h"
#include "renderer/canvas.h"
#include "renderer/render.h"
#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"
#include <cstring>


static Graphics* Divider = nullptr;

static void init() {
    if (Divider == nullptr) {
        Divider = new Graphics("divider");
        Divider->add_input_floats(1, false);
        Divider->vertex_array_binding_divisor = 1;

        Divider->set_vertex_shader(R"(
        #version 410 core

        layout(std140) uniform GlobalData {
            vec4 uFrustum;
            float PixelsToMeters;
            float time;
            ivec2 screenSize;
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

            float x = v.x;
            float y = (12.0 / screenSize.y);   // 12 pixels
            y = y - y / 2 + 0.5;               // centered
            y = v.y;
            gl_Position = vec4(-1.0 + x * 2.0, -1.0 + y * 2.0, 0.0, 1.0);

            uv = v * (vec2(screenSize) / texSize);
        }
        )");

        auto vert = std::string(R"(
        #version 410 core
        layout(std140) uniform Palette { vec4 palette[256]; };
        layout(std140) uniform GlobalData {
            vec4 uFrustum;
            float PixelsToMeters;
            float time;
            ivec2 screenSize;
        };

        in vec2 uv;
        uniform usampler2D tex;
        out vec4 FragColor;

        void main() {
            uint index = texture(tex, uv).r;
            FragColor = palette[index];
        }
        )");

        Divider->set_fragment_shader(vert.c_str());

        Divider->compile();
        Divider->bind_uniform_block(0, "Palette");
        Divider->bind_uniform_block(1, "GlobalData");

        float dummy = 0.0;
        Divider->buffer_data(1, &dummy, GL_STATIC_DRAW);
    }
}



gl_lifecycle<> GlDivider = {
    .init = &init,
    .on_lgr = []() {
        auto pic = LgrTexture.get_qframe();
        Divider->set_texture(0, "tex", pic.tex);
        Divider->uniform2f("texSize", pic.obj->get_width(), pic.obj->get_height());
    },
    .render = [] {
        Divider->draw_instanced(0, 6, 1);
    }
};
