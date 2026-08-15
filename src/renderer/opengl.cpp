
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




void update_shader_globals(float time, driver& driv1) {

    Globals.frustum[0] = (driv1.mot->bike.r.x - SCREEN_WIDTH/2.0 * PixelsToMeters);
    Globals.frustum[1] = (driv1.mot->bike.r.y - SCREEN_HEIGHT/2.0 * PixelsToMeters);
    Globals.frustum[2] = (driv1.mot->bike.r.x + SCREEN_WIDTH/2.0 * PixelsToMeters);
    Globals.frustum[3] = (driv1.mot->bike.r.y + SCREEN_HEIGHT/2.0 * PixelsToMeters);
    for (int i=0; i<4; i++) {
        Globals.frustum[i] = std::floor(Globals.frustum[i] * MetersToPixels) * PixelsToMeters;
    }
    Globals.pixels_to_meters = PixelsToMeters;
    Globals.time = time;

    //printf("frustum %f %f %f %f\n", Globals.frustum[0], Globals.frustum[1], Globals.frustum[2], Globals.frustum[3]);
 
    set_timer_shader_digits(Globals, time);

    glBindBuffer(GL_UNIFORM_BUFFER, GlobalsVBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(shader_globals), &Globals, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, GlobalsVBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}






static char current_lgr_name[30] = {};
static int  current_level_id = 0;



void gl_render_game(double time, driver& driv1, camera& current_camera, GameLoop loop) {

    if (GlobalsVBO == 0) {
        glCreateBuffers(1, &GlobalsVBO);

        Canvas.init();
        Background.init();
        Kuski.init();
        Objects.init();
    }

    if (strcmp(current_lgr_name, CurrentLgrName) != 0) {
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

    update_shader_globals(time, driv1);

    GL_DEBUG
    Background.render();
    GL_DEBUG
    Canvas.render(true);
    GL_DEBUG
    Objects.render();
    GL_DEBUG
    Kuski.render(driv1);
    GL_DEBUG
    Canvas.render(false);
    GL_DEBUG


    SDL_GL_SwapWindow(SDLWindow);
}
