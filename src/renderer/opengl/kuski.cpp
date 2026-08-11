
#include "game/driver.h"
#include "include/pic/lgr.h"
#include "include/pic/pic8.h"
#include <cstring>
#include <glad/glad.h>
#include <cmath>
#include <vector>


static GLuint Program = 0;
static GLuint VAO;
static GLuint VBO;


#define PI 3.1415926535897932
static double BikeFrameX;
static double BikeFrameY;
static vect2 BikeFrameI;
static vect2 BikeFrameJ;
static vect2 BikeFrameR;

static bool StretchEnabled = false;
static double StretchFactor = 1.0;
static vect2 StretchCenter = Vect2i;
static vect2 StretchAxis = Vect2i;





static const char* vert = R"(
#version 430 core
layout (location = 0) in vec2 pos;
out vec2 fragTexCoord;
layout(location = 2) uniform vec4 uFrustrum;
layout(location = 3) uniform mat3 uTransform;

void main() {
  vec3 r = uTransform * vec3(pos, 1.0);

  float x = (r.x-uFrustrum.x)/(uFrustrum.z-uFrustrum.x);
  float y = (r.y-uFrustrum.y)/(uFrustrum.w-uFrustrum.y);

  gl_Position = vec4(-1.0 + x * 2.0, -1.0 + y * 2.0, 0.0, 1.0);

  fragTexCoord = pos;
}
)";

static const char* frag = R"(
#version 430 core
in vec2 fragTexCoord;
out uint FragColor;
layout(location = 4) uniform usampler2D IndexTexture;
layout(location = 6) uniform uint tColor;

void main() {
    uint index = texture(IndexTexture, fragTexCoord).r;
    if (index == tColor) discard;
    FragColor = index;
}
)";


GLuint upload_pcx8(unsigned char* pixels, int width, int height) {
  GLuint tex_id;
  glActiveTexture(GL_TEXTURE2);
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI,
               width, height, 0,
               GL_RED_INTEGER, GL_UNSIGNED_BYTE,
               pixels);
  return tex_id;
}


struct KuskiTex {
  GLuint tex;
  unsigned char transparency;
};

struct gl_bike_pics {
    KuskiTex bike_part1;
    KuskiTex bike_part2;
    KuskiTex bike_part3;
    KuskiTex bike_part4;
    KuskiTex body;
    KuskiTex thigh;
    KuskiTex leg;
    KuskiTex wheel;
    KuskiTex susp1;
    KuskiTex susp2;
    KuskiTex forarm;
    KuskiTex up_arm;
    KuskiTex head;
};

gl_bike_pics GlBike1 = {};

static void init_bike_textures() {

  auto &pics = GlBike1;

  auto tr = Lgr->bike1.wheel->gpixel(0, 0);

  auto upload_bike_texture = [=](pic8* part) {
    auto tex = upload_pcx8(part->get_row(0), part->get_width(), part->get_height());
    return KuskiTex{tex, tr};
  };

  //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  pics.bike_part1 = upload_bike_texture(Lgr->bike1.bike_part1);
  pics.bike_part2 = upload_bike_texture(Lgr->bike1.bike_part2);
  pics.bike_part3 = upload_bike_texture(Lgr->bike1.bike_part3);
  pics.bike_part4 = upload_bike_texture(Lgr->bike1.bike_part4);
  pics.body = upload_bike_texture(Lgr->bike1.body);
  pics.thigh = upload_bike_texture(Lgr->bike1.thigh);
  pics.leg = upload_bike_texture(Lgr->bike1.leg);
  pics.wheel = upload_bike_texture(Lgr->bike1.wheel);
  pics.susp1 = upload_bike_texture(Lgr->bike1.susp1);
  pics.susp2 = upload_bike_texture(Lgr->bike1.susp2);
  pics.forarm = upload_bike_texture(Lgr->bike1.forarm);
  pics.up_arm = upload_bike_texture(Lgr->bike1.up_arm);
  pics.head = upload_bike_texture(Lgr->bike1.head);
  //glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}





template <typename F>
std::vector<unsigned char> render_to_texture(F render) {

    static GLuint fbo = 0;
    static GLuint colorTex = 0;

    if (fbo == 0) {
      // Create color texture
      glGenTextures(1, &colorTex);
      glBindTexture(GL_TEXTURE_2D, colorTex);

      glTexImage2D(
        GL_TEXTURE_2D, 0, GL_R8UI, 800, 800,
        0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr
      );

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      glGenFramebuffers(1, &fbo);
    }


  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }


    GLint prevFbo = 0;
    GLint prevViewport[4];
    GLint prevProgram;
    GLint prevVao;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);



  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }



    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Attach color texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

    // Check completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      printf("Error generating minimap (framebuffer)");
    }

    // Render to offscreen framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 800, 800);
    glClearColor(1, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    GLuint clearValue[] = { 155, 0, 0, 0 };
    glClearBufferuiv(GL_COLOR, 0, clearValue);

  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }

    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    render();
    //glDisable(GL_BLEND);
  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }

    std::vector<unsigned char> pixels(800 * 800);

    // Read pixels from the framebuffer
    //glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadPixels(0, 0, 800, 800, GL_RED_INTEGER, GL_UNSIGNED_BYTE, pixels.data());

  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }
    // Clean up
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
    glBindVertexArray(prevVao);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(prevProgram);

  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }

    return pixels;
}


GLuint compile_program(const char* vert, const char* frag);
void render_bike(gl_bike_pics& texs, const motorst* mot, const bike_metadata* metadata);


void gl_render_kuski(
    bool player1, pic8* pic, double time, vect2 bottomleft_corner,
    const motorst* mot, const bike_metadata* metadata,
    const pic8* shirt
) {
  float frustrum[4];
  printf("%lf %lf\n", bottomleft_corner.x, bottomleft_corner.y);

    frustrum[0] = -12.447917;
    frustrum[1] = -5.0729165;
    frustrum[2] = 7.5104165;
    frustrum[3] = 6.8958335;
  //frustrum[0] = bottomleft_corner.x;
  //frustrum[1] = bottomleft_corner.y;

  //frustrum[2] = frustrum[0] + 1200.0 / 48.0;
  //frustrum[3] = frustrum[1] + 1200.0 / 48.0;


  if (GlBike1.bike_part1.tex == 0) {
      init_bike_textures();
  }

  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }

  if (Program == 0) {

      Program = compile_program(vert, frag);

      VAO = 0;
      VBO = 0;
      glGenVertexArrays(1, &VAO);
      glGenBuffers(1, &VBO);
      glBindVertexArray(VAO);
      glBindBuffer(GL_ARRAY_BUFFER, VBO);

      glVertexArrayVertexBuffer(VAO, 0, VBO, 0, 8);
      glEnableVertexArrayAttrib(VAO, 0);
      glVertexArrayAttribFormat(VAO, 0, 2, GL_FLOAT, false, 0);
      glVertexArrayAttribBinding(VAO, 0, 0);

      float quadUnit[12] = {
        0, 0, 1, 0, 1, 1,
        0, 0, 1, 1, 0, 1,
      };

      glBufferData(GL_ARRAY_BUFFER, 12 * 4, &quadUnit, GL_STATIC_DRAW);
  }

  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }

  auto pixels = render_to_texture([&] {
      glUseProgram(Program);
      glUniform1i(glGetUniformLocation(Program, "IndexTexture"), 2);
      glUniform4f(glGetUniformLocation(Program, "uFrustrum"), frustrum[0], frustrum[1], frustrum[2], frustrum[3]);
      glBindVertexArray(VAO);
      render_bike(GlBike1, mot, metadata);
  });

  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }

  for (int i=0; i<800; i++) {
    memcpy(pic->get_row(i), pixels.data() + i * 800, 800);
  }
}


//GlLifecycle Kuski = {
//  .on_init = &gl_init_kuski,
//  .on_lgr = []{
//    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//    TexHead   = Lgr2->gl_texture("q1head");
//    TexBody   = Lgr2->gl_texture("q1body");
//    TexUparm  = Lgr2->gl_texture("q1up_arm");
//    TexForarm = Lgr2->gl_texture("q1forarm");
//    TexLeg    = Lgr2->gl_texture("q1leg");
//    TexThigh  = Lgr2->gl_texture("q1thigh");
//    TexSusp1  = Lgr2->gl_texture("q1susp1");
//    TexSusp2  = Lgr2->gl_texture("q1susp2");
//    TexWheel  = Lgr2->gl_texture("q1wheel");
//    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
//    TexPart1  = Lgr2->gl_texture("q1bike_part1");
//    TexPart2  = Lgr2->gl_texture("q1bike_part2");
//    TexPart3  = Lgr2->gl_texture("q1bike_part3");
//    TexPart4  = Lgr2->gl_texture("q1bike_part4");
//    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
//  }
//};


static void render_part(vect2 u, vect2 v, vect2 r, int part_id=0) {


  if (StretchEnabled) {
    // Stretch coordinate r
    double distance = (r - StretchCenter) * StretchAxis;
    vect2 delta = (distance * (1.0 - StretchFactor)) * StretchAxis;
    r = r - delta;

    // Stretch coordinate u
    distance = u * StretchAxis;
    delta = (distance * (1.0 - StretchFactor)) * StretchAxis;
    u = u - delta;

    // Stretch coordinate v
    distance = v * StretchAxis;
    delta = (distance * (1.0 - StretchFactor)) * StretchAxis;
    v = v - delta;
  }

  float mat3[9] = {
    float(u.x), float(u.y), 0.0f,
    float(v.x), float(v.y), 0.0f,
    float(r.x), float(r.y), 1.0f
  };
  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }
  glUniformMatrix3fv(glGetUniformLocation(Program, "uTransform"), 1, false, mat3);
  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }

  // It's fast to draw for each part because static buffer
  // In theory it could be one draw call with many textures and an array of mat3s but
  // prob not even worth it
  glDrawArrays(GL_TRIANGLES, 0, 6);
  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }
}

static void render_frame_part(KuskiTex tex, bike_box* box) {

  vect2 u = BikeFrameI * (box->x2 - box->x1);
  vect2 v = BikeFrameJ * (box->y1 - box->y2);
  vect2 r = BikeFrameI * (box->x1 + 260 - BikeFrameX) +
            BikeFrameJ * (BikeFrameY - (box->y1 + 260)) + BikeFrameR;

  glBindTexture(GL_TEXTURE_2D, tex.tex);
  auto loc = glGetUniformLocation(Program, "tColor");
  glUniform1ui(loc, tex.transparency);
  render_part(u, v, r);
}

// Render an affine_pic (remember all affine_pic images are loaded sideways in the lgr)
// All units are in meters
// a = coordinate of middle left of affine_pic position (distal end of the limb)
// b = coordinate of middle right of affine_pic position (proximal end of the limb)
// Along the axis of the vector b->a, displace coordinate a by `a_stretch` meters
// Along the axis of the vector a->b, displace coordinate b by `b_stretch` meters
// height represents the vertical length of the affine_pic (thickness of the limb)
static void render_body_part(KuskiTex tex, vect2 a, vect2 b, double height,
                              double a_stretch, double b_stretch, bool flip,
                              int part_id=0) {

  vect2 i = unit_vector(b - a);
  b = b + i * b_stretch;
  a = a - i * a_stretch;
  vect2 u = b - a;
  vect2 v = flip ? rotate_90deg(i) : rotate_minus90deg(i);
  v = v * height;
  vect2 r = a - v;

  v = v * 2.0f; // Migrating to OpenGL, body parts need this for some reason

  glBindTexture(GL_TEXTURE_2D, tex.tex);
  glUniform1ui(glGetUniformLocation(Program, "tColor"), tex.transparency);
  render_part(u, v, r, part_id);
}

static void render_rigid_part(KuskiTex tex, vect2 r, double radius, double rotation, bool flip,
    int part_id=0) {
  float rad = flip ? -radius : radius;
  vect2 direction(cos(rotation) * rad, sin(rotation) * rad);
  render_body_part(tex, r - direction, r + direction, radius, 0.0, 0.0, flip, part_id);
}

uint64_t xorshift(const uint64_t n,int i){
  return n^(n>>i);
}
uint64_t hash(const uint64_t n){
  uint64_t p = 0x5555555555555555ull; // pattern of alternating 0 and 1
  uint64_t c = 17316035218449499591ull;// random uneven integer constant; 
  return c*xorshift(p*xorshift(n,32),32);
}


void render_bike(gl_bike_pics& texs, const motorst* mot, const bike_metadata* metadata) {

  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }
  // all subsequent tex will be texture2
  glActiveTexture(GL_TEXTURE2);

  double arm_position = metadata->arm_position;
  double turn_phase = metadata->bike_turning.turn_phase;

  // Check to see if bike is turning, and calculate the progress from -1.0 to 1.0 using cos
  bool is_turning = false;
  StretchEnabled = false;
  //if (turn_phase < 0.999) {
  //    is_turning = true;
  //    turn_phase = -cos(turn_phase * PI);
  //}


  // Calculate wheel position relative to screen
  vect2 left_wheel_r = mot->left_wheel.r;
  vect2 right_wheel_r = mot->right_wheel.r;


  // If turning, we will be rendering one wheel in the foreground
  // (usually they are rendered in background)
  bool left_wheel_in_back = true;
  bool right_wheel_in_back = true;
  if (is_turning) {
      if ((turn_phase > 0.0 && !mot->flipped_bike) || (turn_phase <= 0.0 && mot->flipped_bike)) {
          left_wheel_in_back = false;
      } else {
          right_wheel_in_back = false;
      }
  }

  auto flipped = mot->flipped_bike;

  // Render background wheels
  if (left_wheel_in_back) {
    render_rigid_part(texs.wheel, left_wheel_r, mot->left_wheel.radius-.005, mot->left_wheel.rotation,
                     false, 10 ^ flipped);
  }
  if (right_wheel_in_back) {
    render_rigid_part(texs.wheel, right_wheel_r, mot->right_wheel.radius-.005, mot->right_wheel.rotation,
                     false, 11 ^ flipped);
  }


  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }




  // Get the bike position and angle
  vect2 bike_r = mot->bike.r;
  vect2 bike_i = vect2(cos(mot->bike.rotation), sin(mot->bike.rotation));
  vect2 bike_j = rotate_90deg(bike_i);

  // If bike is turning, squish the bike
  if (is_turning) {
      StretchEnabled = true;
      StretchCenter = bike_r;
      bike_i.normalize();
      StretchAxis = bike_i;
      StretchFactor = turn_phase;
  }

  // If the bike is turned, flip the bike
  // Swap the wheels temporarily for the purposes of drawing the suspension
  if (mot->flipped_bike) {
      bike_i = Vect2null - bike_i;
      std::swap(left_wheel_r, right_wheel_r);
  }


  // Bike frame calculations. Rotate the bike frame by 0.62 radians
  BikeFrameX = 390.0;
  BikeFrameY = 420.0;
  constexpr double BIKE_FRAME_ROTATION = 0.62;
  constexpr double BIKE_FRAME_WIDTH = 0.0045;
  BikeFrameI = bike_i * (BIKE_FRAME_WIDTH * cos(BIKE_FRAME_ROTATION)) +
               bike_j * (BIKE_FRAME_WIDTH * sin(BIKE_FRAME_ROTATION));
  BikeFrameJ = rotate_90deg(BikeFrameI);
  if (mot->flipped_bike) {
      BikeFrameJ = Vect2null - BikeFrameJ;
  }
  BikeFrameR = bike_r;


  // Draw susp1
  vect2 susp1_r =
      BikeFrameI * (365.0 - BikeFrameX) + BikeFrameJ * (BikeFrameY - 292.0) + BikeFrameR;
  render_body_part(texs.susp1, left_wheel_r, susp1_r, 0.06, 0.05, 0.03, false);

  // Draw susp2
  vect2 susp2_r =
      BikeFrameI * (370.0 - BikeFrameX) + BikeFrameJ * (BikeFrameY - 520.0) + BikeFrameR;
  render_body_part(texs.susp2, susp2_r, right_wheel_r, 0.06, 0.0, 0.1, false);


  // Draw frame parts
  render_frame_part(texs.bike_part1, &BikeBox1); // tank & bars
  render_frame_part(texs.bike_part2, &BikeBox2); // motor
  render_frame_part(texs.bike_part3, &BikeBox3); // 8
  render_frame_part(texs.bike_part4, &BikeBox4); // mudguard



  // Calculations to draw the kuski
  vect2 body_r = mot->body_r;
  vect2 hip_r = body_r + BikeFrameI * 75.0 + BikeFrameJ * (-47.0);
  vect2 shoulder_r = body_r + BikeFrameI * 47.0 + BikeFrameJ * 65.0;
  vect2 neck_r = body_r + BikeFrameI * 41.0 + BikeFrameJ * 70.0;
  vect2 foot_r =
      BikeFrameI * (346.0 - BikeFrameX) + BikeFrameJ * (BikeFrameY - 514.0) + BikeFrameR;

  // Calculate how to bend the knee based on the hip and foot positions
  // (or how much the king has had to drink)
  vect2 knee_r;
  constexpr double THIGH_LENGTH = 0.51;
  constexpr double LEG_LENGTH = 0.51;
  if (mot->flipped_bike) {
      knee_r = circles_intersection(hip_r, foot_r, THIGH_LENGTH, LEG_LENGTH);
  } else {
      knee_r = circles_intersection(foot_r, hip_r, LEG_LENGTH, THIGH_LENGTH);
  }

  // Draw head
  float HeadRadius = 0.238;
  render_rigid_part(texs.head, mot->head_r, HeadRadius, mot->bike.rotation, mot->flipped_bike);

  // Hand is located on the handlebars, unless we are volting
  vect2 hand_r = susp1_r;
  if (arm_position > 0.0001) {
      // Invert the arm volt percentage to progress from 0->1
      arm_position = 1.0 - arm_position;
      // Left volt + facing left OR right volt + facing left -> Arm goes up
      bool arm_goes_up = true;
      if ((metadata->volt_is_right && !mot->flipped_bike) ||
          (!metadata->volt_is_right && mot->flipped_bike)) {
          // Right volt + facing left OR left volt + facing right -> arm goes down
          arm_goes_up = false;
      }

      // Describe the arm movement for up and down movements
      const double arm_apex_time = arm_goes_up ? 0.25 : 0.2;    // 0.0 to 1.0
      const double max_arm_rotation = arm_goes_up ? 2.7 : -1.6; // radians
      const double max_arm_stretch = arm_goes_up ? -0.3 : 0.15; // meters

      // Calculate arm movement progression away from neutral (0.0 to 1.0)
      double interpolation;
      if (arm_position < arm_apex_time) {
          interpolation = arm_position / arm_apex_time;
      } else {
          interpolation = 1.0 - (arm_position - arm_apex_time) / (1.0 - arm_apex_time);
      }

      // Calculate arm rotation and stretch
      double arm_rotation = max_arm_rotation * interpolation;
      double arm_stretch = max_arm_stretch * interpolation + 1.0;

      // Update hand position based on arm rotation and stretch
      vect2 arm_vector = hand_r - shoulder_r;
      if (!mot->flipped_bike) {
          arm_vector.rotate(-arm_rotation);
      } else {
          arm_vector.rotate(arm_rotation);
      }
      arm_vector = arm_vector * arm_stretch;
      hand_r = shoulder_r + arm_vector;
  }

  // Calculate how to bend the elbow based on shoulder and hand position
  constexpr double FORARM_LENGTH = 0.308 * 1.05;
  constexpr double UP_ARM_LENGTH = 0.328 * 1.05;
  vect2 elbow_r;
  if (mot->flipped_bike) {
      elbow_r = circles_intersection(hand_r, shoulder_r, FORARM_LENGTH, UP_ARM_LENGTH);
  } else {
      elbow_r = circles_intersection(shoulder_r, hand_r, UP_ARM_LENGTH, FORARM_LENGTH);
  }


  // Render body
  render_body_part(texs.thigh, knee_r, hip_r, 0.14, 0.03, 0.1, mot->flipped_bike);
  render_body_part(texs.leg, foot_r, knee_r, 0.21, 0.03, 0.03, mot->flipped_bike);
  const KuskiTex body = texs.body; // shirt ? shirt : bike->body;
  render_body_part(body, hip_r, neck_r, 0.2, 0.1, 0.05, mot->flipped_bike);
  render_body_part(texs.up_arm, elbow_r, shoulder_r, 0.11, 0.08, 0.1, !mot->flipped_bike);
  render_body_part(texs.forarm, hand_r, elbow_r, 0.076, 0.08, 0.1, mot->flipped_bike);



  // Render front wheels
  StretchEnabled = false;
  if (!left_wheel_in_back || !right_wheel_in_back) {
    //glBindTexture(GL_TEXTURE_2D, TexWheel->tex);
    if (mot->flipped_bike) {
      // If we had temporarily inverted the wheels earlier in this function, undo that now
      std::swap(left_wheel_r, right_wheel_r);
    }
    if (!left_wheel_in_back) {
      render_rigid_part(
        texs.wheel, left_wheel_r, mot->left_wheel.radius, mot->left_wheel.rotation, false, 10 ^ flipped
      );
    }
    if (!right_wheel_in_back) {
      render_rigid_part(
        texs.wheel, right_wheel_r, mot->right_wheel.radius, mot->right_wheel.rotation, false, 11 ^ flipped
      );
    }
  }

  { auto err = glGetError(); if (err != 0) { printf("gl error %i @ %i\n", err, __LINE__); } }
}






GLuint compile_program(const char* vert, const char* frag) {

  auto compile_shader = [&](GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
      printf("OpenGL internal error creating shader\n");
      return GLuint(0);
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetShaderInfoLog(shader, 512, nullptr, infoLog);
      printf(
        "shader compilation failed in %s:\n%s\n\n", 
        (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment"),
        infoLog
      );
      glDeleteShader(shader);
      return GLuint(0);
    }
    return shader;
  };

  GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, vert);
  GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, frag);

  if (!vertexShader || !fragmentShader) {
    printf("fail\n");
    return GLuint(0);
  }

  GLuint ShaderProgram = glCreateProgram();
  glAttachShader(ShaderProgram, vertexShader);
  glAttachShader(ShaderProgram, fragmentShader);
  glLinkProgram(ShaderProgram);

  GLint success;
  glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(ShaderProgram, 512, nullptr, infoLog);
    printf("Shader linking failed:\n%s\n\n", infoLog);
    glDeleteProgram(ShaderProgram);
    return GLuint(0);
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return ShaderProgram;
}
