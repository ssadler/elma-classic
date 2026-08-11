#ifndef RENDER_OPENGL_H
#define RENDER_OPENGL_H

#include "game/driver.h"
#include "pic/pic8.h"
#include "vect2.h"
#include <glad/glad.h>



void gl_render_kuski(
    bool player1, pic8* pic, double time, vect2 bottomleft_corner,
    const motorst* mot, const bike_metadata* metadata,
    const pic8* shirt
);

#endif
