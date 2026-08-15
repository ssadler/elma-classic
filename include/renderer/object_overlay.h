#ifndef OBJECT_OVERLAY_H
#define OBJECT_OVERLAY_H

#include "level/object.h"

class pic8;

void init_gravity_arrows();
pic8* get_gravity_arrow(object::Property property);
void draw_gravity_arrow(pic8* pic, int obj_i, int obj_j, object::Property property);
#endif
