#ifndef RENDER_OPENGL_H
#define RENDER_OPENGL_H

#include "eol/eol_types.h"
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
#include <memory>



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

extern gl_lifecycle<RenderKuski> Kuski;
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



struct shader_globals {
    float frustum[4];
    float canvas_pixels_to_meters;
    float time;
    int screen_size[2];
    unsigned int mins;
    unsigned int secs;
    unsigned int csecs;
    float zoom_pixels_to_meters;
};


extern const std::string TimerGLSL;
extern const std::array<int, 10> TimerDigitMasks;
void set_timer_shader_digits(shader_globals& globals, float time);



class OpenGLRenderer : public GameRenderer {
    pic8* pic;
    pic8* pic_view = nullptr;
    public:
    using GameRenderer::GameRenderer;
    void start_frame() override;
    void end_frame() override;
    void render_background() override;
    void subview(int left, int bottom, int right, int top) override;
    void render_back(bool player1) override;
    void render_front(bool player1) override;
    void render_objects(const kuski* spy_kuski) override;
    void render_bike(bool has_flag, const motorst* mot,
                     const bike_metadata* metadata, const bike_pics* bike, const pic8* shirt) override;
    void render_minimap(bool player1, motorst* other_motor,
                        int x1, int y1, int x2, int y2,        
                        vect2 bottomleft_corner, vect2 camera_pos) override;
    void prerender_timers(const char* best_time_text, double flag_tag_time,
                          int dest_width, int dest_height) override;
    void render_info_panel(const std::vector<info_panel_row>& rows) override;
    pic8* get_backbuffer_pic() override;
};



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
