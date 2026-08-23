
#include "pic/lgr.h"
#include "pic/surface.h"
#include "renderer/canvas.h"
#include "renderer/render.h"
#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>


struct span_render_group {
    GraphicsVAO vao;
    GLuint tex;
    float tex_size[2];
    int start;
    int count;
};
struct canvas_painter {
    bool is_back;
    GraphicsProgram gfx;
    std::vector<span_render_group> groups;
    bool loaded;
};

canvas_painter* Back = nullptr;
canvas_painter* Front = nullptr;

static canvas_painter* init_painter(bool is_back) {

    auto paint = new canvas_painter{is_back, GraphicsProgram("canvas"), {}};
    auto gfx = &paint->gfx;

    gfx->vert = R"(
    #version 410 core

    layout(std140) uniform GlobalData {
        vec4 uFrustum;
        float CanvasPixelsToMeters;
        float time;
        ivec2 screenSize;
        int mins;
        int secs;
        int csecs;
        float ZoomPixelsToMeters;
    };

    layout(location = 0) in ivec3 loc;
    layout(location = 1) in ivec3 off;

    uniform vec2 origin;
    uniform vec2 texSize;

    out vec2 uv;

    const vec2 verts[6] = vec2[](
        vec2(0,0), vec2(1,0), vec2(1,1),
        vec2(0,0), vec2(1,1), vec2(0,1)
    );

    void main()
    {
        float p2m = CanvasPixelsToMeters;
        vec2 lo = vec2(loc.x, loc.y) * p2m + origin;
        vec2 hi = lo + vec2(loc.z, 1.0) * p2m;

        vec2 v = verts[gl_VertexID];
        vec2 pos = hi * v + lo * (vec2(1.0)-v);
        vec2 p = (pos - uFrustum.xy) / (uFrustum.zw - uFrustum.xy);
        gl_Position = vec4(-1.0 + p.x * 2.0, -1.0 + p.y * 2.0, 0.0, 1.0);


        // for sprites which are 1d packed into a buffer, interpolate byte offset
        if (texSize.y < 0) {
            float flo = off.x;
            float fhi = off.x + loc.z;
            uv.x = fhi * v.x + flo * (1.0-v.x);
            uv.y = -1;

        // for 2d textures
        } else if (off.z >= 0) {
            lo = vec2(off.x, off.y) / texSize;
            hi = vec2(off.x + loc.z, off.y + 1) / texSize;
            uv = hi * v + lo * (vec2(1.0)-v);

        // for world aligned textures (bg etc)
        } else {
            uv = pos / (texSize * p2m);
        }

    }
    )";

    gfx->frag = std::string(R"(
    #version 410 core
    layout(std140) uniform GlobalData {
        vec4 uFrustum;
        float CanvasPixelsToMeters;
        float time;
        ivec2 screenSize;
        int mins;
        int secs;
        int csecs;
        float ZoomPixelsToMeters;
    };
    layout(std140) uniform Palette { vec4 palette[256]; };

    in vec2 uv;

    uniform usampler2D tex;
    uniform usamplerBuffer sprite;
    uniform vec2 texSize;

    out vec4 FragColor;

    )") + TimerGLSL + R"(

    void main() {

        uint index = texSize.y < 0 ? texelFetch(sprite, int(uv.x)).r : texture(tex, uv).r;
        FragColor = palette[index];

        drawTimer();
    }
    )";

    gfx->compile();
    gfx->bind_uniform_block(0, "Palette");
    gfx->bind_uniform_block(1, "GlobalData");



    return paint;
}


static void reload(canvas_painter* painter) {

    auto canvas = painter->is_back ? CanvasBack : CanvasFront;

    painter->groups.clear();

    auto& gfx = painter->gfx;

    auto origin = canvas->get_origin();
    auto pixels_to_meters = PixelsToMeters;// * GL_ZOOM;
    auto quantize = [=](float f) { return std::floor(f / pixels_to_meters) * pixels_to_meters; };
    gfx.uniform2f("origin", quantize(origin.x), quantize(origin.y));


    auto spans = canvas->export_spans();

    std::sort(
        spans.begin(),
        spans.end(),
        [](auto a, auto b) {
            return std::tie(a.pic_id, a.y, a.x) < std::tie(b.pic_id, b.y, b.x);
        }
    );


    span_render_group* current;
    int last_pic = -0xFFFFFFF;


    for (int i=0; i<spans.size(); i++) {

        auto& span = spans[i];

        if (span.pic_id != last_pic) {
            last_pic = span.pic_id;
            //printf("group id: %li\n", painter->groups.size());

            painter->groups.emplace_back();
            current = &painter->groups.back();
            current->start = i;
            current->vao.add_input_ints(3);
            current->vao.add_input_ints(3);
            current->vao.vertex_array_binding_divisor = 1;
            current->vao.compile();

            if (span.pic_id > 0xFFFFF) {
                GL_DEBUG
                auto cached = LgrTexture.get_qupdown(span.pic_id & 0xFFFFF);
                GL_DEBUG
                //printf("GRASS  %i\n", span.pic_id);
                current->tex = cached.tex;
                current->tex_size[0] = cached.obj->pic->get_width();
                current->tex_size[1] = cached.obj->pic->get_height();
            } else if (span.pic_id > 0xFFFF) {
                GL_DEBUG
                auto cached = LgrTexture.get_picture(span.pic_id & 0xFFFF);
                //auto pic = cached.obj;
                GL_DEBUG
                //printf("PIC %s  %i      %li\n", pic->name, span.pic_id & 0xFFFF, pic->data_len);
                current->tex = cached.tex;
                current->tex_size[0] = cached.obj->data_len;
                current->tex_size[1] = -1;
            } else {
                GL_DEBUG
                auto cached = LgrTexture.get_texture(span.pic_id);
                GL_DEBUG
                //auto pic = cached.obj;
                //printf("TEX %s    %i\n", pic->name, span.pic_id);
                current->tex = cached.tex;
                current->tex_size[0] = cached.obj->pic->get_width();
                current->tex_size[1] = cached.obj->pic->get_height();
            }
        }

        //printf("%i %i %i %i %i %i\n",
        //        span.x,
        //        span.y,
        //        span.width,
        //        span.pic_x,
        //        span.pic_y,
        //        span.pic_id
        //);

        current->count++;
    }

    for (auto& group : painter->groups) {
        group.vao.buffer_data(group.count, &spans[group.start], GL_STATIC_DRAW);
    }

    painter->loaded = true;
}



static void render(canvas_painter* paint) {

    if (!paint->loaded) {
        reload(paint);
    }

    paint->gfx.use();
    paint->gfx.uniform1i("tex", 0);
    paint->gfx.uniform1i("sprite", 1);

    for (auto& group : paint->groups) {

        paint->gfx.uniform2f("texSize", group.tex_size[0], group.tex_size[1]);

        if (group.tex_size[1] <= 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_BUFFER, group.tex & 0xffff);
        } else {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, group.tex & 0xffff);
        }

        group.vao.bind();
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, group.count);
    }
}



gl_lifecycle<bool> Canvas = {
    .init = [] {
        Back = init_painter(true);
        Front = init_painter(false);
    },
    .on_lgr = [] {
        Back->loaded = false;
        Front->loaded = false;
    },
    .on_level = [] {
        Back->loaded = false;
        Front->loaded = false;
    },
    .render = [](bool is_back) {
        render(is_back ? Back : Front);
    }
};
