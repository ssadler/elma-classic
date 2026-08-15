
#include "editor/editor.h"
#include "level/level.h"
#include "level/object.h"
#include "pic/anim.h"
#include "pic/lgr.h"
#include "pic/pic8.h"
#include "renderer/object_overlay.h"
#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"
#include <cstdint>


struct object_painter {
    Graphics gfx;
    lgr_texture_cache::cached_texture<anim> anim_;
};

object_painter* Exit;
object_painter* Killer;
object_painter* Food;



static object_painter* init_painter(std::string name) {

    auto painter = new object_painter{Graphics(std::move(name)), {}};

    const char* vert = R"(
    #version 410 core
    layout(std140) uniform GlobalData {
        vec4 uFrustum;
        float PixelsToMeters;
        float iTime;
    };
    layout (location = 0) in vec2 pos;
    layout (location = 1) in float floatingPhase;
    layout (location = 2) in int gravArrowIn;
    flat out int gravArrow;
    out vec2 fragTexCoord;
    out vec2 uv;

    uniform float frameCount;
    uniform vec2 frameSize;

    const vec2 verts[6] = vec2[](
        vec2(0,0), vec2(1,0), vec2(1,1),
        vec2(0,0), vec2(1,1), vec2(0,1)
    );

    void main() {

        float frame = floor(iTime / 0.014);

        vec2 v = verts[gl_VertexID];
        uv = v;

        fragTexCoord = vec2(v.x, 1.0-v.y);
        fragTexCoord.y /= frameCount;
        fragTexCoord.y += (1.0/frameCount) * mod(frame, frameCount);

        vec2 size = frameSize * PixelsToMeters / 2.0;
        vec2 p = (pos - size) * v + (pos + size) * (vec2(1.0) - v);

        //float dy = -p.y;// + .05 * sin(iTime * 15.5 + floatingPhase);

        float x = (p.x-uFrustum.x)/(uFrustum.z-uFrustum.x);
        float y = (p.y-uFrustum.y)/(uFrustum.w-uFrustum.y);
        
        vec2 ndc = vec2(-1.0 + x * 2.0, -1.0 + y * 2.0);

        gl_Position = vec4(ndc, 0.0, 1.0);

        gravArrow = gravArrowIn;
    }
    )";

    const char* frag = R"(
    #version 410 core
    layout(std140) uniform Palette { vec4 palette[256]; };
    in vec2 fragTexCoord;
    in vec2 uv;
    flat in int gravArrow;
    out vec4 FragColor;
    uniform usampler2D IndexTexture;
    uniform usampler2D GravArrow;


    float sdBox(in vec2 p, in vec2 b) {
        vec2 d = abs(p)-b;
        return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
    }
    float sdTriangleIsosceles( in vec2 p, in vec2 q ) {
        p.x = abs(p.x);
        vec2 a = p - q*clamp( dot(p,q)/dot(q,q), 0.0, 1.0 );
        vec2 b = p - q*vec2( clamp( p.x/q.x, 0.0, 1.0 ), 1.0 );
        float s = -sign( q.y );
        vec2 d = min( vec2( dot(a,a), s*(p.x*q.y-p.y*q.x) ),
                      vec2( dot(b,b), s*(p.y-q.y)  ));
        return -sqrt(d.x)*sign(d.y);
    }
    // https://iquilezles.org/articles/smin
    float smin( float a, float b, float k ) {
        float h = max(k-abs(a-b),0.0);
        return min(a, b) - h*h*0.25/k;
    }


    void main() {
        uint index = texture(IndexTexture, fragTexCoord).r;
        if (index == texture(IndexTexture, vec2(0.0)).r) {
            discard;
        } else {
            FragColor = palette[index];
        }

        if (gravArrow != 0) {

            vec2 uvr = gravArrow == 1 ? uv :
                       gravArrow == 2 ? vec2(uv.x,     1 - uv.y + 0.2) :
                       gravArrow == 3 ? vec2(uv.y - 0, 1 - uv.x + 0.05) :
                                        vec2(uv.y - 0,     uv.x + 0.05);


            float d = smin(
                sdTriangleIsosceles(uvr-vec2(0.5, 0.25), vec2(0.14, 0.28)) - 0.01,
                sdBox(uvr-vec2(0.5, 0.65), vec2(0.035, 0.275)) - 0.01,
                0.02
            );

            if (d < 0) {
                FragColor += vec4(1.0) * 2.0;
                FragColor /= 3.0;
            }
        }
    }
    )";

    auto gfx = &painter->gfx;
    gfx->set_vertex_shader(vert);
    gfx->set_fragment_shader(frag);
    gfx->add_input_floats(2, GL_FALSE);
    gfx->add_input_floats(1, GL_FALSE);
    gfx->add_input_ints(1);
    gfx->vertex_array_binding_divisor = 1;
    gfx->compile();
    gfx->bind_uniform_block(0, "Palette");
    gfx->bind_uniform_block(1, "GlobalData");

    return painter;
}


struct ObjectInputs {
    float x, y, floating_phase;
    int grav_arrow;
};

static ObjectInputs objects_buf[MAX_OBJECTS];

static void upload_verts(object::Type ty, object_painter* painter) {

    int n = 0;

    for (int i = 0; i < MAX_OBJECTS; i++) {

        object* pker = Level->objects[i];
        if (!pker) { break; }
        if (pker->type != ty) { continue; }

        if (ty == object::Type::Food && !pker->active) {
            continue;
        }
        
        objects_buf[n++] = {
            float(pker->r.x),
            float(pker->r.y),
            float(pker->floating_phase),
            (int) pker->property
        };
    }

    painter->gfx.buffer_data(n, objects_buf, GL_STATIC_DRAW);
};


static std::array<uint64_t, 4> foods_cache = {};

void check_upload_foods() {

    std::array<uint64_t, 4> c = {};

    for (int id=0; id<MAX_OBJECTS; id++) {
        auto pker = Level->objects[id];
        if (!pker) { break; }
        if (pker->type != object::Type::Food) {
            continue;
        }
        if (pker->active) {
            c[id/64] |= 1 << (id%64);
        }
    }

    if (c != foods_cache) {
        printf("%li   %li\n", c[0], foods_cache[0]);
        foods_cache = c;
        upload_verts(object::Type::Food, Food);
    }
}



lgr_texture_cache::cached_texture<pic8> GravArrowTex = {};


GlLifecycle<> Objects = {

    .init = [] {
        Exit = init_painter("exit");
        Killer = init_painter("killer");
        Food = init_painter("food");
    },

    .on_lgr = [] {

        for (int i=0; i<3; i++) {

            auto p = i == 0 ? Exit : i == 1 ? Killer : Food;
            p->anim_ = LgrTexture.get_anim(i);
            p->gfx.set_texture(0, "IndexTexture", p->anim_.tex);
            p->gfx.uniform1f("frameCount", p->anim_.obj->frame_count);

            auto& f0 = p->anim_.obj->frames[0];
            p->gfx.uniform2f("frameSize", f0->get_width(), f0->get_height());
        }

        GravArrowTex = LgrTexture.get_grav_arrow();
        Food->gfx.set_texture(1, "GravArrow", GravArrowTex.tex);
    },


    .on_level = [] {

        upload_verts(object::Type::Killer, Killer);
        upload_verts(object::Type::Exit, Exit);
        check_upload_foods();
    },

    .render = [] {

        Exit->gfx.draw_instanced();
        Killer->gfx.draw_instanced();
        check_upload_foods();
        Food->gfx.draw_instanced();
    }
};

