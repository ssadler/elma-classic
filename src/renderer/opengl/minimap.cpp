

#include "pic/pic8.h"
#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"


static const char* vert = R"(
#version 410 core
layout(std140) uniform GlobalData {
    vec4 uFrustum;
    float PixelsToMeters;
    float iTime;
    ivec2 screenSize;
};
layout (location = 0) in vec2 v;
out vec2 fragTexCoord;
uniform vec4 pos_px;


void main() {

    vec2 pos = pos_px.xy * v + pos_px.zw * (1.0 - v);

    pos /= vec2(screenSize);

    gl_Position = vec4(-1 + pos.x * 2, -1 + pos.y * 2, 0, 1);

    fragTexCoord = vec2(-v.x, 1.0-v.y);
}
)";

static const char* frag = R"(
#version 410 core
layout(std140) uniform Palette { vec4 palette[256]; };
in vec2 fragTexCoord;
out vec4 FragColor;
uniform usampler2D IndexTexture;

void main() {
    uint index = texture(IndexTexture, fragTexCoord).r;
    FragColor = palette[index];
}
)";




Graphics* gfx = nullptr;



gl_lifecycle<GLuint, int, int, int, int> GlMinimap = {
    .init = [] {
        gfx = new Graphics("minimap");
        gfx->set_vertex_shader(vert);
        gfx->set_fragment_shader(frag);
        gfx->add_input_ints(2);
        gfx->compile();
        gfx->bind_uniform_block(0, "Palette");
        gfx->bind_uniform_block(1, "GlobalData");

        float quadUnit[12] = {
            0, 0, 1, 0, 1, 1,
            0, 0, 1, 1, 0, 1,
        };

        gfx->buffer_data(6, &quadUnit, GL_STATIC_DRAW);
    },
    .render = [](GLuint tex_id, int x1, int y1, int x2, int y2) {
        gfx->uniform4f("pos_px", x1, y1, x2, y2);
        gfx->set_texture(0, "IndexTexture", tex_id);
        gfx->draw();
    }
};

