#ifndef RENDER_OPENGL_H
#define RENDER_OPENGL_H

#include "game/driver.h"
#include "level/object.h"
#include "pic/anim.h"
#include "pic/lgr.h"
#include "pic/pic8.h"
#include "renderer/grass.h"
#include "renderer/render.h"
#include <functional>
#include <glad/glad.h>
#include <map>



void gl_render_kuski(driver& driv1);


void gl_render_game(double time, driver& driv1, camera& current_camera, GameLoop loop);



GLuint upload_picture_texture(picture* pic);
GLuint upload_pic8_texture(pic8* pic);
GLuint upload_pcx8(unsigned char* pixels, int width, int height, int row_length=0);
GLuint upload_pcx8_cb(int width, int height, const std::function<void(unsigned char*)>& f);


template<typename ...Args>
struct GlLifecycle {
  std::function<void()> init = []{};
  std::function<void()> on_lgr = []{};
  std::function<void()> on_level = []{};
  std::function<void(Args...)> render = []{};
};

extern GlLifecycle<driver&> Kuski;
extern GlLifecycle<bool> Canvas;
extern GlLifecycle<> Objects;
extern GlLifecycle<> Background;



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
        void invalidate();
    private:
        std::map<int, cached_texture<void>> cache;
};

inline lgr_texture_cache LgrTexture;



struct shader_globals {
    float frustum[4];
    float pixels_to_meters;
    float time;
    unsigned int mins;
    unsigned int secs;
    unsigned int csecs;
};


extern const std::string TimerGLSL;
extern const std::array<int, 10> TimerDigitMasks;
void set_timer_shader_digits(shader_globals& globals, float time);



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
