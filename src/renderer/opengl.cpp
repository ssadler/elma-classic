
#include "SDL_video.h"
#include "editor/editor.h"
#include "game/driver.h"
#include "game/game.h"
#include "level/level.h"
#include "physics/init.h"
#include "pic/lgr.h"
#include "pic/surface.h"
#include "renderer/render.h"
#include "renderer/opengl.h"
#include <cstring>




GLuint GlobalsVBO = 0;
shader_globals Globals;
extern SDL_Window* SDLWindow;





static char current_lgr_name[30] = {};
static int  current_level_id = 0;


static void init() {
    if (GlobalsVBO == 0) {
        glGenBuffers(1, &GlobalsVBO);

        Canvas.init();
        Background.init();
        Kuski.init();
        Objects.init();
        GlMinimap.init();
    }

    if (strcmp(current_lgr_name, CurrentLgrName) != 0) {
        printf("gl on LGR: %s\n", CurrentLgrName);
        memcpy(current_lgr_name, CurrentLgrName, 30);

        LgrTexture.invalidate();
        Canvas.on_lgr();
        Background.on_lgr();
        Kuski.on_lgr();
        Objects.on_lgr();
    }

    if (Level->level_id != current_level_id) {
        current_level_id = Level->level_id;

        Canvas.on_level();
        Background.on_level();
        Kuski.on_level();
        Objects.on_level();
    }
}


void OpenGLRenderer::subview(int left, int bottom, int right, int top) {

    if (splitscreen) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(left, bottom, right-left, top-bottom);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }

    //printf("subview %i %i %i %i\n", left, bottom, right, top);

    auto pixels_to_meters = PixelsToMeters * GL_ZOOM;


    auto quantize = [=](float f) { return std::floor(f / pixels_to_meters) * pixels_to_meters; };

    Globals.frustum[0] = quantize(center.x - SCREEN_WIDTH/2.0 * pixels_to_meters);
    Globals.frustum[1] = quantize(center.y - SCREEN_HEIGHT/2.0 * pixels_to_meters);
    Globals.frustum[2] = quantize(center.x + SCREEN_WIDTH/2.0 * pixels_to_meters);
    Globals.frustum[3] = quantize(center.y + SCREEN_HEIGHT/2.0 * pixels_to_meters);

    Globals.screen_size[0] = SCREEN_WIDTH;
    Globals.screen_size[1] = SCREEN_HEIGHT;

    Globals.pixels_to_meters = PixelsToMeters;
    Globals.time = time;

    //printf("frustum %f %f %f %f\n", Globals.frustum[0], Globals.frustum[1], Globals.frustum[2], Globals.frustum[3]);

    glBindBuffer(GL_UNIFORM_BUFFER, GlobalsVBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(shader_globals), &Globals, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, GlobalsVBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


void OpenGLRenderer::start_frame() {
    init();
}
void OpenGLRenderer::end_frame() {
    SDL_GL_SwapWindow(SDLWindow);
}
void OpenGLRenderer::render_background() {
}

void OpenGLRenderer::render_objects(const kuski* spy_kuski) {
    Objects.render(spy_kuski);
    GL_DEBUG
}


void OpenGLRenderer::render_bike(bool has_flag, const motorst* mot,
                 const bike_metadata* metadata, const bike_pics* bike, const pic8* shirt) {
    Kuski.render({mot, metadata, has_flag, bike, shirt});
    GL_DEBUG
}


void OpenGLRenderer::render_back(bool) {
    GL_DEBUG
    Background.render();
    GL_DEBUG
    Canvas.render(true);
    GL_DEBUG
}

void OpenGLRenderer::render_front(bool) {
    Canvas.render(false);
    GL_DEBUG
}

static GLuint minimap_tex = 0;

void OpenGLRenderer::render_minimap(
    bool player1, motorst* other_motor,
    int x1, int y1, int x2, int y2,        
    vect2 bottomleft_corner, vect2 camera_pos
) {
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

void OpenGLRenderer::prerender_timers(const char* best_time_text, double flag_tag_time,
                   int dest_width, int dest_height) {
    set_timer_shader_digits(Globals, time);
}

void OpenGLRenderer::render_info_panel(const std::vector<info_panel_row>& rows) {
    GlConsole.render(rows);
    GL_DEBUG
};
