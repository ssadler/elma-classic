
#ifndef GL_SHADERS_H
#define GL_SHADERS_H

#include "renderer/opengl.h"
#include "main.h"
#include "vect2.h"
#include <cstdio>
#include <string>
#include <vector>
#include <map>



/*
 * Attach reference counted automatic cleanup to GL resources
 */

template<typename Derived>
class GlResource {
    struct Control {
        GLuint id;
        explicit Control(GLuint id) : id(id) {}
        ~Control() { Derived::cleanup(id); }
    };
    std::shared_ptr<Control> resource_;
public:
    explicit GlResource(GLuint id) : resource_(std::make_shared<Control>(id)) {}
    explicit GlResource() : resource_(std::make_shared<Control>(Derived::create())) {}
    explicit operator bool() const noexcept {
        return resource_->id > 0;
    }
    [[nodiscard]] GLuint get() const noexcept {
        return resource_->id;
    }
};
class GlTexture : public GlResource<GlTexture> {
public:
    static void cleanup(GLuint id) noexcept { glDeleteTextures(1, &id); }
    static GLuint create() noexcept { GLuint id = 0; glGenTextures(1, &id); return id; }
};

class GlProgram : public GlResource<GlProgram> {
public: static void cleanup(GLuint id) noexcept { glDeleteProgram(id); }
    static GLuint create() noexcept { return glCreateProgram(); }
};

class GlVAO : public GlResource<GlVAO> {
public: static void cleanup(GLuint id) noexcept { glDeleteVertexArrays(1, &id); }
    static GLuint create() noexcept { GLuint id = 0; glGenVertexArrays(1, &id); return id; }
};

class GlVBO : public GlResource<GlVBO> {
public: static void cleanup(GLuint id) noexcept { glDeleteBuffers(1, &id); }
    static GLuint create() noexcept { GLuint id = 0; glGenBuffers(1, &id); return id; }
};


enum AttribType {
    Int,
    FloatNorm,
    FloatNoNorm
};
struct GlVertexAttributePointer {
 	GLint size;
 	GLenum type;
 	AttribType attribType;
};
struct GlShaderTexture {
    const char* name;
    GLuint texture;
};

class GraphicsProgramCommon {
    virtual GLuint prog() const = 0;
    public:
    void bind_uniform_block(GLuint idx, const char* name) const {
        GLuint index = glGetUniformBlockIndex(prog(), name);
        glUniformBlockBinding(prog(), index, idx);
    }
    void uniform1i(const char* name, int value) const;
    void uniform4i(const char* name, int val0, int val1, int val2, int val3) const;
    void uniform1ui(const char* name, int value) const;
    void uniform1f(const char* name, float value) const;
    void uniform2f(const char* name, float val0, float val1) const;
    void uniform2f(const char* name, vect2 r) const { uniform2f(name, r.x, r.y); }
    void uniform3f(const char* name, float val0, float val1, float val2) const;
    void uniform4f(const char* name, float val0, float val1, float val2, float val3) const;
    void uniform4f(const char* name, float vals[4]) const;
    void uniform2fv(const char* name, size_t count, const float* data) const;
    void uniform_matrix_3fv(const char* name, size_t count, bool normalize, const float* data) const;
};



struct GraphicsVAO {
    std::vector<GlVertexAttributePointer> attribute_pointers;
    int vertex_array_binding_divisor = 0;
    GlVAO vao = GlVAO();
    GlVBO vbo = GlVBO();
    GLint stride;
    int _num_verts = -1;
    int _verts_offset = 0;

    void compile();
    GLint get_stride() {
      GLint stride = 0;
      for (auto &p : attribute_pointers) {
        auto s = p.type == GL_UNSIGNED_BYTE ? 1 : 4;
        stride += p.size * s;
      }
      return stride;
    }

    void add_input_floats(GLint num_vals, GLboolean normalized) {
        auto t = normalized ? AttribType::FloatNorm : AttribType::FloatNoNorm;
        attribute_pointers.push_back({ num_vals, GL_FLOAT, t });
    }

    void add_input_color() {
        attribute_pointers.push_back({ 4, GL_UNSIGNED_BYTE, AttribType::FloatNorm });
    }

    void add_input_ints(GLint num_vals) {
        attribute_pointers.push_back({ num_vals, GL_INT, AttribType::Int });
    }

    void buffer_data(int num_verts, const void* ptr, GLenum usage) {

        if (!vao) {
            internal_error("GraphicsVAO::buffer_data: compile first");
        }

        glBindVertexArray(vao.get());
        glBindBuffer(GL_ARRAY_BUFFER, vbo.get());

        // orphan old buffer
        if (_num_verts > 0) {
            glBufferData(GL_ARRAY_BUFFER, _num_verts * get_stride(), ptr, usage);
        }

        auto size = num_verts * get_stride();
        glBufferData(GL_ARRAY_BUFFER, size, ptr, usage);

        _num_verts = num_verts;
    }
    void bind() const {
        glBindVertexArray(vao.get());
    }
};


struct GraphicsProgram : public GraphicsProgramCommon {
    std::string name;
    std::string frag = "";
    std::string vert = "";
    GlProgram program = GlProgram();

    void compile();
    GraphicsProgram(std::string name) : name(std::move(name)) {}
    void use() const {
        if (!program) {
            internal_error("use: not compiled");
        }
        glUseProgram(program.get());
    }
    GLuint prog() const override { return program.get(); }
};

class Graphics : public GraphicsProgramCommon {
    GraphicsProgram program;
    GraphicsVAO vao;
    std::map<int, GlShaderTexture> textures;
  
  public:
    int vertex_array_binding_divisor = 0;

    Graphics(std::string name) : program(GraphicsProgram(std::move(name))) {}

    void set_fragment_shader(const char* cfrag) {
      program.frag = std::string(cfrag);
    }

    void set_vertex_shader(const char* cvert) {
      program.vert = std::string(cvert);
    }

    void add_input_floats(GLint num_vals, GLboolean normalized) {
        vao.add_input_floats(num_vals, normalized);
    }

    void add_input_color() {
        vao.add_input_color();
    }

    void add_input_ints(GLint num_vals) {
        vao.add_input_ints(num_vals);
    }

    bool compiled() const {
        return program.program.get();
    }

    void buffer_data(int num_verts, const void* ptr, GLenum usage) {
        vao.buffer_data(num_verts, ptr, usage);
    }

    void set_texture(int slot, const char* name, GLuint texture) {
        textures[slot] = { name, texture };
    }


    void compile() {
        program.compile();
        vao.vertex_array_binding_divisor |= vertex_array_binding_divisor;
        vao.compile();
    }
    void use() const {
        program.use();
    }
    void draw(GLenum mode, GLint first, GLsizei count) {
      init_draw();
      glDrawArrays(mode, first, count);
    }
    void draw(GLint first, GLsizei count) {
      draw(GL_TRIANGLES, first, count);
    }
    void draw() {
      if (vao._num_verts > 0) {
        draw(vao._verts_offset, vao._num_verts);
      }
    }
    void draw_indexed(GLsizei count, GLenum type, const void * indices) {
      init_draw();
      glDrawElements(GL_TRIANGLES, count, type, indices);
    }
    void draw_instanced(GLint first, GLsizei count, GLsizei instancecount) {
      init_draw();
      glDrawArraysInstanced(GL_TRIANGLES, first, count, instancecount);
    }
    void draw_instanced() {
      if (vao._num_verts > 0) {
          //printf("draw_instanced()  %s   %i\n", name.data(), _num_verts);
          init_draw();
          glDrawArraysInstanced(GL_TRIANGLES, 0, 6, vao._num_verts);
      }
    }

    void init_draw();

    GLuint prog() const override { return program.program.get(); }

  private:

    void _compile_program();
};


#endif
