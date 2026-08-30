#ifndef RENDER_OPENGL_H
#define RENDER_OPENGL_H

#include "game/driver.h"
#include "pic/anim.h"
#include "pic/lgr.h"
#include "pic/pic8.h"
#include "renderer/grass.h"
#include "renderer/render.h"
#include <cassert>
#include <functional>
#include <glad/glad.h>
#include <map>
#include <string>



void gl_render_kuski(driver& driv1);


void gl_render_game(double time, driver& driv1, camera& current_camera, GameLoop loop,
    const std::vector<info_panel_row>& info_rows
);



GLuint upload_picture_texture(const picture* pic);
GLuint upload_pic8_texture(const pic8* pic);
GLuint upload_pcx8(unsigned char* pixels, int width, int height, int row_length=0);
GLuint upload_pcx8_cb(int width, int height, const std::function<void(unsigned char*)>& f);

struct RenderKuski {
    const motorst* mot;
    const bike_metadata* metadata;
    bool has_flag;
    const bike_pics* bike;
    const pic8* shirt;
};

template<typename ...Args>
struct gl_lifecycle {
  std::function<void()> init = []{};
  std::function<void()> on_lgr = []{};
  std::function<void()> on_level = []{};
  std::function<void(Args...)> render = []{};
};

extern gl_lifecycle<> Kuski;
extern gl_lifecycle<bool> Canvas;
extern gl_lifecycle<const kuski*> Objects;
extern gl_lifecycle<> Background;
extern gl_lifecycle<GLuint, int, int, int, int> GlMinimap;
extern gl_lifecycle<> GlDivider;



class lgr_texture_cache {
    public:
        template <typename T>
        struct cached_texture {
            GLuint tex;
            T* obj;
        };
        cached_texture<texture> get_texture(int texture_id);
        cached_texture<updown>  get_qupdown(int qupdown_id);
        cached_texture<picture> get_picture(int picture_id);
        cached_texture<anim>    get_anim(int anim_id);
        cached_texture<pic8>    get_grav_arrow();
        cached_texture<pic8>    get_rider_flag();
        cached_texture<pic8>    get_qframe();
        void invalidate();
    private:
        cached_texture<pic8>    get_misc_tex(int id, pic8* pic);
        std::map<int, cached_texture<void>> cache;
};

inline lgr_texture_cache LgrTexture;



/*
 * Be a bit careful about struct padding, for example a glsl vec2 should be
 * on an 8 byte offset.
 */
struct shader_globals {
    float frustum[4];
    int screen_size[2];
    float bottomleft_corner[2];
    int canvas_corner[2];
    float canvas_pixels_to_meters;
    float time;
    float zoom_pixels_to_meters;
    unsigned int mins, secs, csecs;
};
const std::string SHADER_GLOBALS = R"(
#version 410 core
struct Globals {
    vec4  frustum;
    ivec2 screenSize;
    vec2  bottomleft_corner;
    ivec2 canvas_corner;
    float PixelsToMeters;
    float time;
    float ZoomPixelsToMeters;
    int   mins;
    int   secs;
    int   csecs;
};
layout(std140) uniform GlobalData {
    Globals globals;
};
)";


extern const std::string TimerGLSL;
extern const std::array<int, 10> TimerDigitMasks;
void set_timer_shader_digits(shader_globals& globals, float time);




void opengl_bike_draw_affine_pic(
    const pic8* affine, unsigned char transparency, vect2 u, vect2 v, vect2 r);

GameRenderer* createOpenGLRenderer(
        double time, driver& driv1, driver& driv2, camera& current_camera, GameLoop loop);


#ifdef DEBUG
#define GL_DEBUG \
    { auto err = glGetError(); \
        if (err != 0) { \
            printf("### GL ERROR %i @ %s:%i\n", err, __FILE__, __LINE__); \
        } }
#else
#define GL_DEBUG {}
#endif





#endif
