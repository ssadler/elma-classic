//#include "gl_renderer.h"
//#include "affine_pic.h"
#include "pic/lgr.h"
#include "pic/pic8.h"
#include "renderer/opengl.h"

#include <functional>
#include <glad/glad.h>
#include <cstring>


GLuint upload_pcx8(unsigned char* pixels, int width, int height, int row_length) {
  GLuint tex_id;
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  if (row_length > 0) {
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
  }
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI,
               width, height, 0,
               GL_RED_INTEGER, GL_UNSIGNED_BYTE,
               pixels);

  if (row_length > 0) {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  }

  return tex_id;
}







GLuint upload_pcx8_cb(int width, int height, const std::function<void(unsigned char*)>& f) {

  auto size = width * height;

  GLuint tex;
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &tex);

  // upload from PBO
  glBindTexture(GL_TEXTURE_2D, tex);

  GLuint pbo;
  glGenBuffers(1, &pbo);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);

  glBufferData(GL_PIXEL_UNPACK_BUFFER, size, nullptr, GL_STREAM_DRAW);

  // map buffer
  void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
  f((unsigned char*)ptr);
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI,
               width, height, 0,
               GL_RED_INTEGER, GL_UNSIGNED_BYTE,
               0);

  //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  GL_DEBUG

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

  return tex;
}



GLuint upload_pic8_texture(const pic8* pic) {

    auto w = pic->get_width();

    return upload_pcx8_cb(w, pic->get_height(), [&](unsigned char* ptr) {
        for (int y=0; y<pic->get_height(); y++) {
            memcpy(ptr + w * y, pic->get_row(y), w);
        }
    });
}


GLuint upload_picture_texture(const picture* pic) {


    GLuint buffer_id;
    GL_DEBUG
    glGenBuffers(1, &buffer_id);
    GL_DEBUG
    glBindBuffer(GL_TEXTURE_BUFFER, buffer_id);
    GL_DEBUG

    glBufferData(GL_TEXTURE_BUFFER, pic->data_len, pic->data, GL_STATIC_DRAW);
    GL_DEBUG

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    GL_DEBUG
    glBindTexture(GL_TEXTURE_BUFFER, tex_id);
    GL_DEBUG

    glTexBuffer(GL_TEXTURE_BUFFER, GL_R8UI, buffer_id);
    GL_DEBUG

    glBindBuffer(GL_TEXTURE_BUFFER, 0);
    GL_DEBUG
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    GL_DEBUG

    return tex_id | (buffer_id << 16);
}



//GLuint upload_picture_texture_(picture* pic) {
//    return upload_pcx8_lgr2(pic->width, pic->height, [&](unsigned char* ptr) {
//
//        // Draw the picture
//
//        int offset = 0;
//        for (int y=0; y<pic->height; y++) {
//            int j = 0;
//            while (true) {
//                int skip = read_varint(pic->data, offset);
//                if (skip == -1) {
//                    ptr += pic->width - j;
//                    break;
//                }
//                memset(ptr, 128, skip);
//                ptr += skip;
//                j += skip;
//
//                int count = read_varint(pic->data, offset);
//                memcpy(ptr, &pic->data[offset], count);
//                offset += count;
//                ptr += count;
//                j += count;
//            }
//        }
//    });
//}
//
//
//
//
//GLuint upload_rgba(unsigned char* pixels, int width, int height) {
//
//  GLuint tex_id;
//  glActiveTexture(GL_TEXTURE0);
//  glGenTextures(1, &tex_id);
//  glBindTexture(GL_TEXTURE_2D, tex_id);
//  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//glActiveTexture(GL_TEXTURE0);
//  //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
//               width, height, 0,
//               GL_RGBA, GL_UNSIGNED_BYTE,
//               pixels);
//  return tex_id;
//}


