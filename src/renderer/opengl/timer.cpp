
#include "renderer/timer.h"
#include "renderer/opengl.h"
#include <array>



/*
 * The timer is not it's own renderer, reason being that
 * the digits invert the color beneath them and this isn't
 * easy and/or performant to do in dedicated draw calls,
 * so it's implemented as a filter and included by other shaders.
 */



/*
 *     1
 *  2     4 
 *     8
 *  16   32
 *     64
 */

const std::array<int, 10> TimerDigitMasks = {
    /* 0 */  1 | 2 | 4 |     16 | 32 | 64,
    /* 1 */          4 |          32,
    /* 2 */  1 |     4 | 8 | 16 |      64,
    /* 3 */  1 |     4 | 8 |      32 | 64,
    /* 4 */  2 |     4 | 8 |      32,
    /* 5 */  1 | 2 |     8 |      32 | 64,
    /* 6 */  1 | 2 |     8 | 16 | 32 | 64,
    /* 7 */  1 |     4 |          32,
    /* 8 */  1 | 2 | 4 | 8 | 16 | 32 | 64,
    /* 9 */  1 | 2 | 4 | 8 |      32 | 64,
};


void set_timer_shader_digits(shader_globals& globals, float time) {

    int t = (int)(time * TIME_TO_CENTISECONDS);

    globals.csecs = TimerDigitMasks[t % 10] << 7;
    t /= 10;
    globals.csecs += TimerDigitMasks[t % 10];
    t /= 10;

    globals.secs = TimerDigitMasks[t % 10] << 7;
    t /= 10;
    globals.secs += TimerDigitMasks[t % 6];
    t /= 6;

    globals.mins = TimerDigitMasks[t % 10] << 7;
    t /= 10;
    globals.mins += TimerDigitMasks[t % 10];
}


const std::string TimerGLSL = R"(

float sdBox(in vec2 p, in vec2 b) {
    vec2 d = abs(p)-b;
    return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

void invert() {
    float c = FragColor.r + FragColor.g + FragColor.b;
    if (c < 1.5) {
        FragColor = vec4(1.0);
    } else {
        FragColor = vec4(vec3(0.0), 1.0);
    }
}


void drawColon(vec2 refPos) {

    float d = sdBox(refPos, vec2(4, 20));
    if (d > 0.0) return;
    //FragColor += vec4(0.1, 0.1, 0.1, 0.0); // show area


    if (sdBox(refPos + vec2(0, 12), vec2(1)) < 0) {
        invert();
    } else if (sdBox(refPos - vec2(0, 12), vec2(1)) < 0) {
        invert();
    }
}


void drawDigit(vec2 refPos, int mask) {

    refPos += vec2(0.5);

    const float h = 16;
    const float w = 10;
    const float t = 0.5;

    float d = sdBox(refPos, vec2(w + 4, h * 2 + 4));
    if (d > 0.0) return;
    //FragColor += vec4(0.1, 0.1, 0.1, 0.0); // show area
    


#define digitLeg(m, off) \
    if ((mask & m) > 0 && sdBox(refPos + off, dims) < 0.0) { \
        invert(); return; \
    }

    vec2 dims = vec2(w, t);
    digitLeg(1,  vec2(0.0,  h*2+2));
    digitLeg(8,  vec2(0.0,    0.0));
    digitLeg(64, vec2(0.0, -h*2-2));

    dims = vec2(t, h);
    digitLeg(2,  vec2(-w-1,  h+1));
    digitLeg(16, vec2(-w-1, -h-1));
    digitLeg(4,  vec2( w+1,  h+1));
    digitLeg(32, vec2( w+1, -h-1));
}


void drawTimer() {
    vec2 refPos = vec2(1700.0, 1050.0) - gl_FragCoord.xy;

    // Skip if not in bounding box

    float d = sdBox(refPos, vec2(140.0, 60.0));
    if (d > 0.0) return;
    //FragColor += vec4(0.1, 0.1, 0.1, 0.0); // show area


    // Draw timer

    drawDigit(refPos + vec2(-110.0, 0.0), mins & 127);
    drawDigit(refPos + vec2(-74.0, 0.0), mins >> 7);

    drawColon(refPos + vec2(-46, 0));

    drawDigit(refPos + vec2(-18.0, 0.0), secs & 127);
    drawDigit(refPos + vec2(18.0, 0.0), secs >> 7);

    drawColon(refPos + vec2(46, 0));

    drawDigit(refPos + vec2(74.0, 0.0), csecs & 127);
    drawDigit(refPos + vec2(110.0, 0.0), csecs >> 7);
}
)";
