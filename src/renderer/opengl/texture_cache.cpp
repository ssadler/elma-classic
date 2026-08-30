
#include "pic/anim.h"
#include "pic/lgr.h"
#include "renderer/object_overlay.h"
#include "renderer/opengl.h"
#include "renderer/grass.h"
#include <cassert>
#include <cstring>
#include <map>


static const int PICTURE_MASK = (1<<16);
static const int GRASS_MASK = (1<<17);
static const int ANIM_MASK = (1<<18);
static const int MISC_MASK = (1<<19);
static const int GRAVITY_ARROW = MISC_MASK;
static const int RIDER_FLAG = (MISC_MASK + 1);
static const int QFRAME = (MISC_MASK + 1);
static const int DEFAULT_BACKGROUND = -2;
static const int DEFAULT_FOREGROUND = -1;


void lgr_texture_cache::invalidate() {
    for (auto [_, o] : cache) {
        glDeleteTextures(1, &o.tex);
    }
    cache.clear();
}

template <typename T>
using cached = lgr_texture_cache::cached_texture<T>;

cached<texture> lgr_texture_cache::get_texture(int texture_id) {

#ifdef DEBUG
    assert(texture_id >= DEFAULT_BACKGROUND);
    assert(texture_id < Lgr->texture_count);
#endif

    if (texture_id == DEFAULT_BACKGROUND) {
        texture_id = Lgr->get_texture_index(Lgr->background_name);
    } else if (texture_id == DEFAULT_FOREGROUND) {
        texture_id = Lgr->get_texture_index(Lgr->foreground_name);
    }

    auto r = &cache[texture_id];

    if (r->tex == 0) {
        auto texture = &Lgr->textures[texture_id];
        r->tex = upload_pic8_texture(texture->pic);
        r->obj = (void*) texture;
    }

    return {r->tex, (texture*)r->obj};
}

cached<updown> lgr_texture_cache::get_qupdown(int qupdown_id) {

#ifdef DEBUG
    assert(qupdown_id < Lgr->grass_pics->elements.size());
#endif

    //auto id = qupdown_id < 0 ? qupdown_id : (qupdown_id | GRASS_MASK);
    auto id = qupdown_id | GRASS_MASK;
    auto r = &cache[id];

    if (r->tex == 0) {
        auto texture = &Lgr->grass_pics->elements[qupdown_id];
        r->tex = upload_pic8_texture(texture->pic.get());
        r->obj = (void*)texture;
    }

    return {r->tex, (updown*)r->obj};
}

cached<picture> lgr_texture_cache::get_picture(int picture_id) {

#ifdef DEBUG
    assert(picture_id < Lgr->picture_count);
#endif

    auto r = &cache[picture_id | PICTURE_MASK];

    if (r->tex == 0) {
        auto picture = &Lgr->pictures[picture_id];
        r->tex = upload_picture_texture(picture);
        r->obj = (void*)picture;
    }

    return {r->tex & 0xFFFF, (picture*)r->obj};
}



cached<anim> lgr_texture_cache::get_anim(int anim_id) {

#ifdef DEBUG
    assert(anim_id <= 2);
#endif

    auto r = &cache[anim_id | ANIM_MASK];

    if (r->tex == 0) {
        auto anim = anim_id == 0 ? Lgr->exit :
            anim_id == 1 ? Lgr->killer :
            Lgr->food[0];

        auto w = anim->frames[0]->get_width();
        auto h = anim->frames[0]->get_height();
        r->tex = upload_pcx8_cb(w, h * anim->frame_count, [&](auto ptr) {
            for (int i=0; i<anim->frame_count; i++) {
                for (int y=0; y<h; y++) {
                    memcpy(ptr, anim->frames[i]->get_row(y), w);
                    ptr += w;
                }
            }
        });
        r->obj = (void*)anim;
    }

    return {r->tex, (anim*)r->obj};
}


cached<pic8> lgr_texture_cache::get_misc_tex(int id, pic8* pic) {
    auto r = &cache[id];

    if (r->tex == 0) {
        r->tex = upload_pic8_texture(pic);
        r->obj = (void*) pic;
    }

    return {r->tex, (pic8*)r->obj};
}

cached<pic8> lgr_texture_cache::get_grav_arrow() {
    return get_misc_tex(GRAVITY_ARROW, get_gravity_arrow(object::Property::GravityDown));
}

cached<pic8> lgr_texture_cache::get_rider_flag() {
    return get_misc_tex(RIDER_FLAG, Lgr->flag);
}

cached<pic8> lgr_texture_cache::get_qframe() {
    return get_misc_tex(QFRAME, Lgr->qframe);
}
