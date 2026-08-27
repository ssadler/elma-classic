
#include "SDL_video.h"
#include "editor/editor.h"
#include "game/driver.h"
#include "game/game.h"
#include "level/level.h"
#include "physics/init.h"
#include "pic/lgr.h"
#include "pic/surface.h"
#include "platform/implementation.h"
#include "platform/sdl/gl_renderer.h"
#include "renderer/render.h"
#include "renderer/opengl.h"
#include <cstring>




GLuint GlobalsVBO = 0;
shader_globals Globals;
extern SDL_Window* SDLWindow;


static GLuint minimap_tex = 0;



static char current_lgr_name[30] = {};
static int  current_level_id = 0;


static void init_renderers() {
    if (GlobalsVBO == 0) {
        glGenBuffers(1, &GlobalsVBO);

        Canvas.init();
        Background.init();
        Kuski.init();
        Objects.init();
        GlMinimap.init();
        GlDivider.init();
    }

    if (strcmp(current_lgr_name, CurrentLgrName) != 0) {
        memcpy(current_lgr_name, CurrentLgrName, 30);

        LgrTexture.invalidate();
        Canvas.on_lgr();
        Background.on_lgr();
        Kuski.on_lgr();
        Objects.on_lgr();
        GlDivider.on_lgr();
    }

    if (Level->level_id != current_level_id) {
        current_level_id = Level->level_id;

        Canvas.on_level();
        Background.on_level();
        Kuski.on_level();
        Objects.on_level();
        GlDivider.on_level();
    }
}




void _render_info_panel(pic8* pic, const std::vector<info_panel_row>& rows);




class OpenGLRenderer : public GameRenderer {
    pic8* pic;
    pic8* pic_view = nullptr;
    public:
    using GameRenderer::GameRenderer;








void subview(int left, int bottom, int right, int top) override {

    // For overlays
    delete pic_view;
    pic_view = new pic8;
    pic_view->subview(left, bottom, right, top, pic);

    auto pixels_to_meters = PixelsToMeters * GlZoom;
    float adjusted_y = center.y;


    if (splitscreen) {
        // Bottom view is already correct placed because scissor doesnt change aspect ratio and
        // bottomleft_corner is actually at the bottom, but top needs to be shifted up
        //
        if (bottom > 0) {
            auto height_rel = 1.0 - (top-bottom) / float(SCREEN_HEIGHT);
            adjusted_y -= SCREEN_HEIGHT * pixels_to_meters * height_rel;
        }
        glEnable(GL_SCISSOR_TEST);
        glScissor(left, bottom, right-left, top-bottom);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }


    auto quantize = [=](float f) { return std::floor(f / pixels_to_meters) * pixels_to_meters; };

    Globals.frustum[0] = quantize(center.x - SCREEN_WIDTH/2.0 * pixels_to_meters);
    Globals.frustum[1] = quantize(adjusted_y - SCREEN_HEIGHT/2.0 * pixels_to_meters);
    Globals.frustum[2] = Globals.frustum[0] + SCREEN_WIDTH * pixels_to_meters;
    Globals.frustum[3] = Globals.frustum[1] + SCREEN_HEIGHT * pixels_to_meters;

    Globals.screen_size[0] = SCREEN_WIDTH;
    Globals.screen_size[1] = SCREEN_HEIGHT;

    Globals.canvas_pixels_to_meters = PixelsToMeters;
    Globals.zoom_pixels_to_meters = pixels_to_meters;
    Globals.time = time;

    glBindBuffer(GL_UNIFORM_BUFFER, GlobalsVBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(shader_globals), &Globals, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, GlobalsVBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void start_frame() override {

    /*
     * We provide the backbuffer pic for EOL overlays,
     * but unlike the regular renderer, it does not already contain
     * the rendered view and will be drawn to the framebuffer
     * as a texture.
     *
     * The issue with this is that we don't have a fixed transparency color
     * available so that we can only overlay pixels that were actually drawn.
     *
     * Just choose a random one for now
     */
    pic = lock_backbuffer_pic(true);
    pic->fill_box(150);
    gl_presenter_transparency(150);


    init_renderers();

    if (splitscreen) {
        glDisable(GL_SCISSOR_TEST);
        GlDivider.render();
    }
}



void end_frame() override {
    unlock_backbuffer_pic();
}

void render_background() override {
}

void render_objects(const kuski* spy_kuski) override {
    Objects.render(spy_kuski);
    GL_DEBUG
}


void render_back(bool) override {
    GL_DEBUG
    Background.render();
    GL_DEBUG
    Canvas.render(true);
    GL_DEBUG
}

void render_front(bool) override {
    Canvas.render(false);
    GL_DEBUG
}


void render_minimap(
    bool player1, motorst* other_motor,
    int x1, int y1, int x2, int y2,        
    vect2 bottomleft_corner, vect2 camera_pos
) override {
    if (minimap_tex) {
        glDeleteTextures(1, &minimap_tex);
    }

    auto width = x2-x1+1;
    auto height = y2-y1+1;

    minimap_tex = upload_pcx8_cb(width+2, height+2, [&](unsigned char* ptr) {

        memset(ptr, Lgr->minimap_border_palette_id, (width+2)*(height+2));

        pic8 pic;
        auto pitch = width + 2;
        pic.subview(width, height, ptr + pitch + 1, pitch, false);

        render_minimap_subview(player1, &pic, other_motor, bottomleft_corner, camera_pos);
    });

    GlMinimap.render(minimap_tex, x1, y1, x2, y2);
};

void prerender_timers(const char* best_time_text, double flag_tag_time,
                   int dest_width, int dest_height) override {
    set_timer_shader_digits(Globals, time);
}

void render_info_panel(const std::vector<info_panel_row>& rows) override {
    _render_info_panel(pic_view, rows);
};

pic8* get_backbuffer_pic() override {
    return pic;
}


    void bike_draw_affine_pic(const pic8* affine, unsigned char transparency,
            vect2 u, vect2 v, vect2 r) override {

        opengl_bike_draw_affine_pic(affine, transparency, u, v, r);
    }




};





GameRenderer* createOpenGLRenderer(
        double time, driver& driv1, driver& driv2, camera& current_camera, GameLoop loop) {
    return new OpenGLRenderer(time, driv1, driv2, current_camera, loop);
}

