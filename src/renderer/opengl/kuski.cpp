
#include "include/pic/lgr.h"
#include "include/pic/pic8.h"
#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"
#include <cstring>
#include <glad/glad.h>
#include <cmath>
#include <unordered_map>



#define PI 3.1415926535897932

static Graphics* Painter = nullptr;

//static double BikeFrameX;
//static double BikeFrameY;
//static vect2 BikeFrameI;
//static vect2 BikeFrameJ;
//static vect2 BikeFrameR;

static bool StretchEnabled = false;
static double StretchFactor = 1.0;
static vect2 StretchCenter = Vect2i;
static vect2 StretchAxis = Vect2i;




static const char* vert = R"(
#version 410 core
layout(std140) uniform GlobalData {
    vec4 uFrustum;
    float PixelsToMeters;
};
layout (location = 0) in vec2 pos;
out vec2 fragTexCoord;
uniform mat3 uTransform;

void main() {
  vec3 r = uTransform * vec3(pos, 1.0);

  float x = (r.x-uFrustum.x)/(uFrustum.z-uFrustum.x);
  float y = (r.y-uFrustum.y)/(uFrustum.w-uFrustum.y);

  gl_Position = vec4(-1.0 + x * 2.0, -1.0 + y * 2.0, 0.0, 1.0);


  fragTexCoord = pos;
}
)";

static const char* frag = R"(
#version 410 core
layout(std140) uniform Palette { vec4 palette[256]; };
in vec2 fragTexCoord;
out vec4 FragColor;
uniform usampler2D IndexTexture;
uniform uint tColor;

void main() {
    uint index = texture(IndexTexture, fragTexCoord).r;
    if (index == tColor) discard;
    FragColor = palette[index];
}
)";


static std::unordered_map<const pic8*, GLuint> BikeTextures;



static void init() {

    GL_DEBUG

    Painter = new Graphics("kuski");
    Painter->set_fragment_shader(frag);
    Painter->set_vertex_shader(vert);
    Painter->add_input_floats(2, false);
    Painter->compile();

    Painter->bind_uniform_block(0, "Palette");
    Painter->bind_uniform_block(1, "GlobalData");

    Painter->uniform1i("IndexTexture", 0);

    float quadUnit[12] = {
        0, 0, 1, 0, 1, 1,
        0, 0, 1, 1, 0, 1,
    };

    Painter->buffer_data(6, &quadUnit, GL_STATIC_DRAW);
    GL_DEBUG
}


void opengl_bike_draw_affine_pic(
    const pic8* affine, unsigned char transparency, vect2 u, vect2 v, vect2 r) {

    auto& tex = BikeTextures[affine];

    if (!tex) {
        tex = upload_pic8_texture(affine);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);


    if (StretchEnabled) {
        // Stretch coordinate r
        double distance = (r - StretchCenter) * StretchAxis;
        vect2 delta = (distance * (1.0 - StretchFactor)) * StretchAxis;
        r = r - delta;

        // Stretch coordinate u
        distance = u * StretchAxis;
        delta = (distance * (1.0 - StretchFactor)) * StretchAxis;
        u = u - delta;

        // Stretch coordinate v
        distance = v * StretchAxis;
        delta = (distance * (1.0 - StretchFactor)) * StretchAxis;
        v = v - delta;
    }


    float mat3[9] = {
        float(u.x), float(u.y), 0.0f,
        float(v.x), float(v.y), 0.0f,
        float(r.x), float(r.y), 1.0f
    };


    Painter->uniform_matrix_3fv("uTransform", 1, false, mat3);
    Painter->uniform1ui("tColor", transparency);
    Painter->draw();
    GL_DEBUG
}





gl_lifecycle<> Kuski = {
    .init = init,
    .on_level = [] {
        for (auto [_, tex] : BikeTextures) {
            glDeleteTextures(1, &tex);
        }
        BikeTextures.clear();
    }
};

