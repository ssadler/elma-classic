#ifndef RENDER_H
#define RENDER_H

#include "eol/eol.h"
#include "pic/lgr.h"
#include "pic/pic8.h"
#include "vect2.h"
#include <cassert>
#include <string>
class pic8;
struct bike_metadata;
struct camera;
struct driver;

// Boundary of the screen with a slight buffer - used to render affine pics
extern double AffinePicScreenLeft, AffinePicScreenRight, AffinePicScreenBottom, AffinePicScreenTop;

extern int GameViewWidth, GameViewHeight;

void init_renderer();
void reset_game_background();

void increase_view_size();
void decrease_view_size();

enum class GameLoop { Game, Replay, Render };
void render_game(double time, driver& driv1, driver& driv2, camera& current_camera, GameLoop loop);

inline float GlZoom = 1.0;

struct info_panel_row {
    std::string label;
    std::string value;
};


/*
 * GameRenderer abstract superclass
 */
class GameRenderer {
    protected:
    bool draw_player1;
    bool draw_player2;
    bool splitscreen;
    double time;
    driver& driv1;
    driver& driv2;
    camera& current_camera;
    GameLoop loop;
    vect2 bottomleft_corner;
    vect2 center;

    public:
    virtual void start_frame() = 0;
    virtual void end_frame() = 0;
    virtual void render_background(bool scale_changed) = 0;
    virtual void subview(int left, int bottom, int right, int top) = 0;
    virtual void render_back(bool player1) = 0;
    virtual void render_front(bool player1) = 0;
    virtual void render_objects(const kuski* spy_kuski) = 0;
    virtual void render_minimap(
            bool player1, motorst* other_motor,
            int x1, int y1, int x2, int y2,
            vect2 bottomleft_corner, vect2 camera_pos
    ) = 0;
    virtual void render_timers(const char* best_time_text, double flag_tag_time,
                               int dest_width, int dest_height) {}
    virtual void prerender_timers(const char* best_time_text, double flag_tag_time,
                                  int dest_width, int dest_height) {}
    virtual pic8* get_backbuffer_pic() = 0;

    virtual void bike_draw_affine_pic(const pic8* affine, unsigned char transparency,
            vect2 u, vect2 v, vect2 r) = 0;

    void render_view(bool player1, bool bottom_player, int left, int bottom, int right, int top);
    void render_bike(bool has_flag, const motorst* mot, const bike_metadata* metadata,
            const bike_pics* bike, const pic8* shirt);
    void dispatch_minimap(bool player1, double camera_turn_phase, vect2 bike_center,
                          motorst* other_motor);

    GameRenderer(double time, driver& driv1, driver& driv2, camera& current_camera, GameLoop loop);
    virtual ~GameRenderer() = default;

    void render();
};


GameRenderer* createPicRenderer(
        double time, driver& driv1, driver& driv2, camera& current_camera, GameLoop loop);


void render_minimap_subview(bool player1, pic8* minimap_view, motorst* other_motor,
    vect2 bottomleft_corner, vect2 camera_pos);


#endif
