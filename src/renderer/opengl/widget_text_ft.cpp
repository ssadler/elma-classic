#include <iostream>
#include <map>
#if defined(_WIN32)
#define LOAD_FONT_SIZE load_font_size_h
#else
#define FT_LOAD 1
#define LOAD_FONT_SIZE load_font_size_ft
#endif

#include <cstdio>
#include <cstdlib>
#include <format>
#include <fstream>

#include "renderer/opengl_widget.h"

#include "stb_image.h"

#ifndef FT_LOAD
#include "../build/font_atlas.h"
#include "../build/font_ranges.h"
#endif


#ifndef _WIN32
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "robotomono.h"
//#include "include/notosanssymbols.h"
#endif

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>










std::map<int, font> Fonts;
using codepoint_ranges = std::vector<std::tuple<uint32_t, uint32_t>>;
codepoint_ranges ranges;

uint16_t ATLAS_COLS = 80;
uint16_t ATLAS_ROWS = 11;


struct Atlas {
    int width;
    int height;
    std::vector<uint8_t> pixels; // grayscale
};



font upload_font_texture(int width, int height, const uint8_t* pixels) {
  GLuint fontTexture;
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &fontTexture);
  glBindTexture(GL_TEXTURE_2D, fontTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);


  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
               width, height, 0,
               GL_RED, GL_UNSIGNED_BYTE,
               pixels);


  return font {
    fontTexture,
    width / ATLAS_COLS,
    height / ATLAS_ROWS
  };
}






#ifdef FT_LOAD

Atlas build_font_atlas(
  FT_Face& face,
  int pixelHeight,
  float weight
) {

  constexpr float hscale = 1.2;

    FT_Set_Pixel_Sizes(face, 0, pixelHeight);

    FT_Matrix matrix;
    matrix.xx = (1 << 16) * hscale;   // 1.10 = 110%
    matrix.xy = 0;
    matrix.yx = 0;
    matrix.yy = (1 << 16);          // 1.0  (no vertical scaling)
    FT_Set_Transform(face, &matrix, nullptr);


    FT_MM_Var* mm_var;
    FT_Get_MM_Var(face, &mm_var);

    //for (FT_UInt i = 0; i < mm_var->num_axis; i++) {
    //    auto axis = mm_var->axis[i];

    //    std::cout << "Axis: "
    //              << axis.name
    //              << " (" << axis.tag << ") "
    //              << "min=" << axis.minimum
    //              << " max=" << axis.maximum
    //              << " default=" << axis.def
    //              << "\n";
    //}

    float t = (weight - 100.0f) / (700.0f - 100.0f);
    FT_Fixed wght =
        mm_var->axis[0].minimum +
        t * (mm_var->axis[0].maximum - mm_var->axis[0].minimum);

    FT_Fixed coords[1] = { wght };
    FT_Set_Var_Design_Coordinates(face, 1, coords);


    // ---------------------------
    // First pass
    // ---------------------------
    int cellWidth = 0;
    int n_codepoints = 0;

    for (auto &[a, b] : ranges) {
      for (uint32_t c = a; c <= b; ++c) {
        n_codepoints++;
        auto flags = FT_LOAD_RENDER;
        if (FT_Load_Char(face, c, flags)) continue;
        cellWidth = std::max(cellWidth, (int)(face->glyph->advance.x >> 6));
      }
    }

    if (cellWidth == 0) cellWidth = pixelHeight;
    //cellWidth *= hscale;


    // ----------------------------
    // Font metrics (correct baseline model)
    // ----------------------------
    int cellHeight = face->size->metrics.height >> 6; // aka lineHeight
    int baselineOffset = (face->size->metrics.ascender >> 6) - 1;


    // ----------------------------
    // Atlas size
    // ----------------------------
    auto cols = ATLAS_COLS;
    int rows = (n_codepoints + cols - 1) / cols;
    if (rows != ATLAS_ROWS) {
      printf("rows (%i) != ATLAS_ROWS, needs updating\n", rows);
      exit(1);
    }
    ATLAS_ROWS = rows;
    int atlasW = cols * cellWidth;
    int atlasH = rows * cellHeight;

    Atlas atlas;
    atlas.width = atlasW;
    atlas.height = atlasH;
    atlas.pixels.resize(atlasW * atlasH, 0);

    // ----------------------------
    // Render glyphs
    // ----------------------------
    int i=0;
    for (auto &[a, b] : ranges) {
      for (uint32_t c = a; c <= b; ++c) {

        FT_GlyphSlot g;
        auto flags = FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT | FT_LOAD_FORCE_AUTOHINT;
        if (FT_Load_Char(face, c, flags) != 0) { continue; }
        g = face->glyph;


        int x0 = (i % cols) * cellWidth;
        int y0 = (i / cols) * cellHeight;

        int baseline = y0 + baselineOffset;
        int gx = x0 + g->bitmap_left;
        //int gy = baseline - g->bitmap_top;
        int gy = baseline - (g->metrics.horiBearingY >> 6);

        // ----------------------------
        // Copy bitmap safely
        // ----------------------------
        for (int y = 0; y < g->bitmap.rows; ++y) {
            int dstY = gy + y;
            if (dstY < 0 || dstY >= atlasH) continue;

            uint8_t* dst = &atlas.pixels[dstY * atlasW + gx];
            const uint8_t* src = &g->bitmap.buffer[y * g->bitmap.pitch];

            int copyW = std::min((int)g->bitmap.width, atlasW - gx);
            if (copyW > 0)
                std::memcpy(dst, src, copyW);
        }

        i++;
      }
    }

    return atlas;
}

codepoint_ranges get_unicode_ranges(FT_Face face) {
    codepoint_ranges ranges;

    if (!face || !face->charmap)
        return ranges;

    uint32_t start = 0;
    uint32_t prev  = 0;
    bool in_range  = false;

    uint32_t gindex;
    uint32_t charcode = FT_Get_First_Char(face, &gindex);

    while (gindex != 0) {
        // Only accept valid mapped glyphs
        if (gindex != 0) {
            if (!in_range) {
                start = charcode;
                prev = charcode;
                in_range = true;
            } else if (charcode == prev + 1) {
                prev = charcode;
            } else {
                ranges.emplace_back(start, prev);
                start = charcode;
                prev = charcode;
            }
        }

        charcode = FT_Get_Next_Char(face, charcode, &gindex);
    }

    if (in_range)
        ranges.emplace_back(start, prev);

    return ranges;
}




font load_font(FT_Face& face, int pixelHeight, int weight) {

  auto init_ranges = ranges.empty();
  if (init_ranges) {
    ranges = get_unicode_ranges(face);
  }

  auto r = build_font_atlas(face, pixelHeight, weight);

  auto path = std::getenv("ELMA_SOURCE_DIR");
  printf("source dir %s\n", path);

  if (path && path[0]) {
    std::string filename = std::format("{}/build/font-atlas-{}-{}.png", path, pixelHeight, weight);
    if (stbi_write_png(filename.c_str(), r.width, r.height, 1, r.pixels.data(), r.width)) {
       std::cout << "Saved font atlas: " << filename << std::endl;
    } else {
        printf("error saving font atlas\n");
    }
    if (init_ranges) {
      // write ranges
      std::ofstream r(std::format("{}/build/font_ranges.h", path));
      r << "#include <vector>\n#include <tuple>\n#include <cstdint>\n";
      r << "std::vector<std::tuple<uint32_t, uint32_t>> _atlas_ranges = {";
      for (auto [lo, hi] : ranges) {
        r << "{" << lo << ", " << hi << "},";
      }
      r << "};\n";
      r.close();
    }
  }

  return upload_font_texture(r.width, r.height, r.pixels.data());
}



void load_font_size_ft(int size) {
  if (Fonts.contains(size)) return;

  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
      throw std::runtime_error("FT_Init_FreeType failed");
  }

  FT_Face face;
  if (FT_New_Memory_Face(ft,
        resources_RobotoMono_VariableFont_wght_ttf, 
        resources_RobotoMono_VariableFont_wght_ttf_len,
        0, &face)
      ) {
      throw std::runtime_error("FT_New_Face failed");
  }

  Fonts[size] = load_font(face, size, 400);
  Fonts[1000+size] = load_font(face, size, 700);

  FT_Done_Face(face);
  FT_Done_FreeType(ft);
}

#else

void load_font_size_h(int size) {
  if (Fonts.contains(size)) return;

  if (ranges.empty()) {
    ranges = _atlas_ranges;
  }

  
  auto f = [&](int weight) {
    auto png = font_atlases.at(std::format("{}_{}", size, weight));
    int width, height, c;
    stbi_uc* pixels = stbi_load_from_memory(png->data(), png->size(), &width, &height, &c, 1);
    auto r = upload_font_texture(width, height, pixels);
    stbi_image_free(pixels);
    return r;
  };
  Fonts[size] = f(400);
  Fonts[1000+size] = f(700);
}

#endif


font& get_font(int size, bool bold) {
  LOAD_FONT_SIZE(size);
  return Fonts[size + (bold ? 1000 : 0)];
}

font& get_font(int size) {
  return get_font(size, false);
}


