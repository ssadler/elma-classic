
#include "game/driver.h"
#include "include/pic/lgr.h"
#include "include/pic/pic8.h"
#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"
#include <cstring>
#include <glad/glad.h>
#include <cmath>
#include <unordered_map>



#define PI 3.1415926535897932

static Graphics* Painter = nullptr;

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
#version 410 core
layout(std140) uniform GlobalData {
    vec4 uFrustum;
    float PixelsToMeters;
};
layout (location = 0) in vec2 pos;
out vec2 fragTexCoord;
uniform mat3 uTransform;

void main() {
  vec3 r = uTransform * vec3(pos, 1.0);

  float x = (r.x-uFrustum.x)/(uFrustum.z-uFrustum.x);
  float y = (r.y-uFrustum.y)/(uFrustum.w-uFrustum.y);

  gl_Position = vec4(-1.0 + x * 2.0, -1.0 + y * 2.0, 0.0, 1.0);


  fragTexCoord = pos;
}
)";

static const char* frag = R"(
#version 410 core
layout(std140) uniform Palette { vec4 palette[256]; };
in vec2 fragTexCoord;
out vec4 FragColor;
uniform usampler2D IndexTexture;
uniform uint tColor;

void main() {
    uint index = texture(IndexTexture, fragTexCoord).r;
    if (index == tColor) discard;
    FragColor = palette[index];
}
)";



struct KuskiTex {
  GLuint tex;
  unsigned char transparency;
};

#define APPLY_TO_BIKE_PARTS(code) \
    code(bike_part1)              \
    code(bike_part2)              \
    code(bike_part3)              \
    code(bike_part4)              \
    code(body)                    \
    code(thigh)                   \
    code(leg)                     \
    code(wheel)                   \
    code(susp1)                   \
    code(susp2)                   \
    code(forarm)                  \
    code(up_arm)                  \
    code(head)

#define DEFINE_BIKE_PART(name) KuskiTex name;
#define UPLOAD_BIKE_PART_TEX(name) dest.name = {upload_pic8_texture(src->name), tr};
#define FREE_BIKE_PART_TEX(name) glDeleteTextures(1, &dest.name.tex);

struct gl_bike_pics {
    APPLY_TO_BIKE_PARTS(DEFINE_BIKE_PART)
};


gl_bike_pics GlBike1 = {};
gl_bike_pics GlBike2 = {};

static std::unordered_map<const bike_pics*, gl_bike_pics> BikeTextures;
static std::unordered_map<const pic8*, KuskiTex> Shirts;


static void upload_bike_textures(const bike_pics* src, gl_bike_pics& dest) {
    auto tr = src->wheel->gpixel(0, 0);
    APPLY_TO_BIKE_PARTS(UPLOAD_BIKE_PART_TEX);
}

static void free_bike_textures(gl_bike_pics& dest) {
    APPLY_TO_BIKE_PARTS(FREE_BIKE_PART_TEX);
}

#undef APPLY_TO_BIKE_PARTS
#undef DEFINE_BIKE_PART
#undef UPLOAD_BIKE_PART_TEX
#undef FREE_BIKE_PART_TEX



static void init() {

    GL_DEBUG

    Painter = new Graphics("kuski");
    Painter->set_fragment_shader(frag);
    Painter->set_vertex_shader(vert);
    Painter->add_input_floats(2, false);
    Painter->compile();

    Painter->bind_uniform_block(0, "Palette");
    Painter->bind_uniform_block(1, "GlobalData");

    Painter->uniform1i("IndexTexture", 0);

    float quadUnit[12] = {
        0, 0, 1, 0, 1, 1,
        0, 0, 1, 1, 0, 1,
    };

    Painter->buffer_data(6, &quadUnit, GL_STATIC_DRAW);
    GL_DEBUG
}




static void render_part(vect2 u, vect2 v, vect2 r) {

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

    Painter->uniform_matrix_3fv("uTransform", 1, false, mat3);
    Painter->draw();
    GL_DEBUG
}

static void render_frame_part(KuskiTex tex, bike_box* box) {

    vect2 u = BikeFrameI * (box->x2 - box->x1);
    vect2 v = BikeFrameJ * (box->y1 - box->y2);
    vect2 r = BikeFrameI * (box->x1 + 260 - BikeFrameX) +
                        BikeFrameJ * (BikeFrameY - (box->y1 + 260)) + BikeFrameR;

    GL_DEBUG
    glBindTexture(GL_TEXTURE_2D, tex.tex);
    GL_DEBUG
    Painter->uniform1ui("tColor", tex.transparency);
    GL_DEBUG
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
                             double a_stretch, double b_stretch, bool flip) {

    vect2 i = unit_vector(b - a);
    b = b + i * b_stretch;
    a = a - i * a_stretch;
    vect2 u = b - a;
    vect2 v = flip ? rotate_90deg(i) : rotate_minus90deg(i);
    v = v * height;
    vect2 r = a - v;

    v = v * 2.0f; // Migrating to OpenGL, body parts need this for some reason

    GL_DEBUG
    glBindTexture(GL_TEXTURE_2D, tex.tex);
    GL_DEBUG
    Painter->uniform1ui("tColor", tex.transparency);
    GL_DEBUG
    render_part(u, v, r);
    GL_DEBUG
}

static void render_rigid_part(KuskiTex tex, vect2 r, double radius, double rotation, bool flip) {
    float rad = flip ? -radius : radius;
    vect2 direction(cos(rotation) * rad, sin(rotation) * rad);
    render_body_part(tex, r - direction, r + direction, radius, 0.0, 0.0, flip);
}

uint64_t xorshift(const uint64_t n,int i){
    return n^(n>>i);
}
uint64_t hash(const uint64_t n){
    uint64_t p = 0x5555555555555555ull; // pattern of alternating 0 and 1
    uint64_t c = 17316035218449499591ull;// random uneven integer constant; 
    return c*xorshift(p*xorshift(n,32),32);
}


void render_bike(RenderKuski k) {

    auto mot = k.mot;
    auto metadata = k.metadata;
    auto has_flag = k.has_flag;

    auto& texs = BikeTextures[k.bike];
    if (texs.bike_part1.tex == 0) {
        upload_bike_textures(k.bike, texs);
    }

    KuskiTex shirt;
    if (k.shirt) {
        auto& s = Shirts[k.shirt];
        if (!s.tex) {
            s = {upload_pic8_texture(k.shirt), k.shirt->gpixel(0, 0)};
        }
        shirt = s;
    } else {
        shirt = texs.body;
    }



    GL_DEBUG
    // all subsequent tex will be texture
    glActiveTexture(GL_TEXTURE0);
    GL_DEBUG

    double arm_position = metadata->arm_position;
    double turn_phase = metadata->bike_turning.turn_phase;

    // Check to see if bike is turning, and calculate the progress from -1.0 to 1.0 using cos
    bool is_turning = false;
    StretchEnabled = false;
    //if (turn_phase < 0.999) {
    //        is_turning = true;
    //        turn_phase = -cos(turn_phase * PI);
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

    // Render background wheels
    if (left_wheel_in_back) {
        render_rigid_part(texs.wheel, left_wheel_r, mot->left_wheel.radius-.005, mot->left_wheel.rotation,
                                         false);
    }
    if (right_wheel_in_back) {
        render_rigid_part(texs.wheel, right_wheel_r, mot->right_wheel.radius-.005, mot->right_wheel.rotation,
                                         false);
    }


    GL_DEBUG


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

    // Draw flagtag flag
    if (has_flag) {
        vect2 flag_base_r = BikeFrameI * (500.0 + 107 - BikeFrameX) +
                            BikeFrameJ * (BikeFrameY + 114 - 600.0) + BikeFrameR;
        vect2 flag_tip_r = flag_base_r + (BikeFrameI * 356.0 + BikeFrameJ * 500.0) * 0.2;
        
        auto [tex, pic] = LgrTexture.get_rider_flag();
        KuskiTex flag = {tex, pic->gpixel(0, 0)};
        render_body_part(flag, flag_base_r, flag_tip_r, 0.2, 0.0, 0.0, mot->flipped_bike);
    }

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
    // (or how much the his majesty has had to drink)
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
            const double arm_apex_time = arm_goes_up ? 0.25 : 0.2;        // 0.0 to 1.0
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
    GL_DEBUG
    render_body_part(texs.leg, foot_r, knee_r, 0.21, 0.03, 0.03, mot->flipped_bike);
    GL_DEBUG
    render_body_part(shirt, hip_r, neck_r, 0.2, 0.1, 0.05, mot->flipped_bike);
    GL_DEBUG
    render_body_part(texs.up_arm, elbow_r, shoulder_r, 0.11, 0.08, 0.1, !mot->flipped_bike);
    GL_DEBUG
    render_body_part(texs.forarm, hand_r, elbow_r, 0.076, 0.08, 0.1, mot->flipped_bike);


    GL_DEBUG

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
                texs.wheel, left_wheel_r, mot->left_wheel.radius, mot->left_wheel.rotation, false
            );
        }
        if (!right_wheel_in_back) {
            render_rigid_part(
                texs.wheel, right_wheel_r, mot->right_wheel.radius, mot->right_wheel.rotation, false
            );
        }
    }

    GL_DEBUG
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




gl_lifecycle<RenderKuski> Kuski = {
    .init = init,
    .on_lgr = [] {
        for (auto [_, tex] : BikeTextures) {
            free_bike_textures(tex);
        }
    },
    .on_level = [] {
        for (auto [_, tex] : Shirts) {
            glDeleteTextures(1, &tex.tex);
        }
    },
    .render = [](RenderKuski k) {
        render_bike(k);
    }
};

