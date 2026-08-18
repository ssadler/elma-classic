
#ifndef GL_SHADERS_H
#define GL_SHADERS_H

#include "renderer/opengl.h"
#include "main.h"
#include "vect2.h"
#include <cstdio>
#include <tuple>
#include <string>
#include <vector>
#include <functional>
#include <map>





enum AttribType {
  Int,
  FloatNorm,
  FloatNoNorm
};


struct GlVertexAttributePointer {
  GLuint vbo;
 	GLint size;
 	GLenum type;
 	AttribType attribType;
};
struct GlShaderTexture {
  const char* name;//GLuint loc;
  GLuint texture;
};


class Graphics {
  std::string name;
  std::string frag;
  std::string vert;
  std::vector<GlVertexAttributePointer> attribute_pointers;
  std::vector<std::function<void()>> draw_cbs;
  std::map<int, GlShaderTexture> textures;
  std::map<std::string, GLuint, std::less<>> _locations;

  
  public:
    GLuint program = 0;
    int _num_verts = -1;
    int _verts_offset = 0;
    GLuint vao = 0;
    GLuint vbo = 0;
    int vertex_array_binding_divisor = 0;

    Graphics(std::string shader_name) : name(std::move(shader_name)) {
      glGenBuffers(1, &vbo);
    }
    ~Graphics() {
      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);
    }
    
    /*
     * Clone allows you to copy the whole structure in order to assign a different
     * data buffer
     */
    //GlManaged* clone() {
    //  return clone(this->name);
    //}
    //GlManaged* clone(std::string shader_name) {
    //  GlManaged* s = new GlManaged(*this);
    //  glGenBuffers(1, &s->vbo);
    //  s->name = std::move(shader_name);
    //  s->vao = 0;
    //  s->_compile_vao();
    //  return s;
    //}

    void set_fragment_shader(const char* cfrag) {
      frag = std::string(cfrag);
    }

    void set_vertex_shader(const char* cvert) {
      vert = std::string(cvert);
    }

    void add_input_floats(GLint num_vals, GLboolean normalized) {
      auto t = normalized ? AttribType::FloatNorm : AttribType::FloatNoNorm;
      attribute_pointers.push_back({ vbo, num_vals, GL_FLOAT, t });
    }

    void add_input_color() {
      attribute_pointers.push_back({ vbo, 4, GL_UNSIGNED_BYTE, AttribType::FloatNorm });
    }

    void add_input_ints(GLint num_vals) {
      attribute_pointers.push_back({ vbo, num_vals, GL_INT, AttribType::Int });
    }

    void bind_uniform_block(GLuint idx, const char* name) const {
        GLuint index = glGetUniformBlockIndex(program, name);
        glUniformBlockBinding(program, index, idx);
    }

    bool compiled() const {
        return program != 0;
    }

    void buffer_data(int num_verts, const void* ptr, GLenum usage) {
      if (!vao) {
        internal_error("GlManaged::buffer_data: compile first");
      }

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);


      // orphan old buffer
      if (_num_verts > 0) {
          glBufferData(GL_ARRAY_BUFFER, _num_verts * get_stride(), ptr, usage);
      }

      auto size = num_verts * get_stride();
      glBufferData(GL_ARRAY_BUFFER, size, ptr, usage);

      _num_verts = num_verts;
    }

    void sub_data(int offset, int num_verts, void* ptr) {
      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferSubData(GL_ARRAY_BUFFER, offset, num_verts * get_stride(), ptr);
    }

    void set_texture(int slot, const char* name, GLuint texture) {
      textures[slot] = { name, texture };
    }

    void add_draw_cb(std::function<void()> cb) {
      draw_cbs.push_back(std::move(cb));
    }

    GLuint loc(const char* name) const {
      if (_locations.contains(name)) {
        return _locations.at(name);
      }
      return glGetUniformLocation(program, name);
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

    void use() const {
      if (!program) {
        internal_error("use: not compiled");
      }
      glUseProgram(program);
    }
    void compile();
    void draw(GLenum mode, GLint first, GLsizei count) {
      init_draw();
      glDrawArrays(mode, first, count);
    }
    void draw(GLint first, GLsizei count) {
      draw(GL_TRIANGLES, first, count);
    }
    void draw() {
      if (_num_verts > 0) {
        draw(_verts_offset, _num_verts);
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
      if (_num_verts > 0) {
          //printf("draw_instanced()  %s   %i\n", name.data(), _num_verts);
          init_draw();
          glDrawArraysInstanced(GL_TRIANGLES, 0, 6, _num_verts);
      }
    }

    void init_draw();

  private:

    GLint get_stride() {
      GLint stride = 0;
      for (auto &p : attribute_pointers) {
        auto s = p.type == GL_UNSIGNED_BYTE ? 1 : 4;
        stride += p.size * s;
      }
      return stride;
    }

    GLuint _compile_shader();
    GLuint _compile_program() const;
    void _compile_vao();
};



template <typename T,
          typename TIter = decltype(std::begin(std::declval<T>())),
          typename = decltype(std::end(std::declval<T>()))>
constexpr auto enumerate(T && iterable)
{
    struct iterator
    {
        std::size_t i;
        TIter iter;
        bool operator != (const iterator & other) const { return iter != other.iter; }
        void operator ++ () { ++i; ++iter; }
        auto operator * () const { return std::tie(i, *iter); }
    };
    struct iterable_wrapper
    {
        T iterable;
        auto begin() { return iterator{ 0, std::begin(iterable) }; }
        auto end() { return iterator{ 0, std::end(iterable) }; }
    };
    return iterable_wrapper{ std::forward<T>(iterable) };
}




class gl_ring_buffer {
  const int N_FRAMES = 3;
  int stride;
  int buftype;
  int max_verts;
  int offset = 0;

  public:
  GLuint vbo;

  gl_ring_buffer(int _buftype, int _stride, int _max_verts)
    : gl_ring_buffer(
        _buftype, _stride, _max_verts,
        []{ GLuint vbo; glGenBuffers(1, &vbo); return vbo; }()
      ) {}

  gl_ring_buffer(int _buftype, int _stride, int _max_verts, GLuint _vbo)
    : stride(_stride), buftype(_buftype), max_verts(_max_verts), vbo(_vbo) {
    glBindBuffer(buftype, vbo);
    glBufferData(buftype, max_verts * N_FRAMES * stride, nullptr, GL_STREAM_DRAW);
  }

  int push_data(int num_verts, void* ptr) {
    if (num_verts > max_verts) {
      internal_error("gl_ring_buffer::push_data: num_verts > max_verts");
    }
    auto size = num_verts * stride;
    if (offset + size > max_verts * stride * N_FRAMES) {
      offset = 0;
    }
    glBindBuffer(buftype, vbo);
    glBufferSubData(buftype, offset, size, ptr);
    int o = offset;
    offset += size;
    return o / stride;
  }
};




#endif
