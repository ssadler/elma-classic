
#include "platform/sdl/gl_renderer.h"
#include "renderer/canvas.h"
#include "renderer/render.h"
#include "renderer/opengl.h"
#include <cstring>


/*
 * Since we are subclassing pic_renderer we can include it here.
 * Methods definitions of classes are implicitly inline so we
 * just need to indicate that we don't want other non class top levels.
 */
#define RENDER_PIC_IS_INCLUDE
#include "render_pic.cpp"



static GLuint GlobalsVBO = 0;
static shader_globals Globals;




class OpenGLRenderer : public PicRenderer {

    public:
    using PicRenderer::PicRenderer;

    void subview(int left, int bottom, int right, int top) override {

        /*
         * Inform superclass
         */
        PicRenderer::subview(left, bottom, right, top);

        /*
         * Set render view
         */

        auto gl_p2m = PixelsToMeters * GlZoom;
        float adjusted_y = center.y;

        if (splitscreen) {
            // Bottom view is already correct placed because scissor doesnt change viewprt (just cuts it)
            // and bottomleft_corner is actually at the bottom, but top needs to be shifted up.
            if (bottom > 0) {
                auto height_rel = 1.0 - (top-bottom) / float(SCREEN_HEIGHT);
                adjusted_y -= SCREEN_HEIGHT * gl_p2m * height_rel;
            }
            glEnable(GL_SCISSOR_TEST);
            glScissor(left, bottom, right-left, top-bottom);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }


        auto quantize = [=](float f) { return std::floor(f / gl_p2m) * gl_p2m; };

        Globals.frustum[0] = quantize(center.x - SCREEN_WIDTH/2.0 * gl_p2m);
        Globals.frustum[1] = quantize(adjusted_y - SCREEN_HEIGHT/2.0 * gl_p2m);
        Globals.frustum[2] = Globals.frustum[0] + SCREEN_WIDTH * gl_p2m;
        Globals.frustum[3] = Globals.frustum[1] + SCREEN_HEIGHT * gl_p2m;

        /*
         * Update other shader globals
         */

        Globals.screen_size[0] = SCREEN_WIDTH;
        Globals.screen_size[1] = SCREEN_HEIGHT;

        Globals.bottomleft_corner[0] = bottomleft_corner.x;
        Globals.bottomleft_corner[1] = bottomleft_corner.y;

        Globals.canvas_pixels_to_meters = PixelsToMeters;
        Globals.zoom_pixels_to_meters = gl_p2m;
        Globals.time = time;

        CanvasBack->meters_to_pixels(bottomleft_corner, &Globals.canvas_corner[0], &Globals.canvas_corner[1]);

        /*
         * Push shader globals
         */

        glBindBuffer(GL_UNIFORM_BUFFER, GlobalsVBO);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(shader_globals), &Globals, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, GlobalsVBO);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void start_frame() override {


        /*
         * We provide the backbuffer pic for EOL overlays,
         * but unlike the pic renderer, it will not contain
         * the whole view and will be drawn to the framebuffer
         * as a texture.
         *
         * The issue with this is that we don't have a fixed transparency color
         * available so that we can only overlay pixels that were actually drawn.
         *
         * Just choose a random one for now
         */
        PicRenderer::start_frame();
        pic_main->fill_box(150);
        gl_presenter_transparency(150);



        /*
         * Initialize renderers if required
         */
        {
            static char current_lgr_name[30] = {};
            static int  current_level_id = 0;

            if (GlobalsVBO == 0) {
                glGenBuffers(1, &GlobalsVBO);

                Canvas.init();
                Background.init();
                Kuski.init();
                Objects.init();
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
    }

    void render_qframe(bool) override {
        /*
         * Render divider for splitscreen.
         */
        if (splitscreen) {
            glDisable(GL_SCISSOR_TEST);
            GlDivider.render();
        }
    }

    void render_objects() override {
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

    void prerender_timers(const char* best_time_text, double flag_tag_time,
                       int dest_width, int dest_height) override {
        set_timer_shader_digits(Globals, time);
    }
    void render_timers(const char* best_time_text, double flag_tag_time,
                               int dest_width, int dest_height) override {
        /* noop */
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

