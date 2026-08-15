
#include "main.h"

#include <cstring>
#include <string>
#include <regex>
#include <string_view>
#include <string>

#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"



void Graphics::compile() {
  if (program) {
    internal_error("Graphics::compile: already compiled");
  }

  std::string name = this->name;
  program = _compile_shader();

  if (program == -1) {
    internal_error("Graphics::compile: failed creating program");
  }
  _compile_vao();
}

GLuint Graphics::_compile_shader() {


  if (frag.empty()) {
    internal_error("Graphics::compile: frag empty");
  }
  if (vert.empty()) {
    internal_error("Graphics::compile: vert empty");
  }
  if (attribute_pointers.empty()) {
    internal_error("Graphics::compile: attribute pointers not set");
  }

  auto prog = _compile_program();
  if (!prog) {
    exit(1);
  }

  auto set_locations = [this](const char* source) {
    std::match_results<std::string_view::const_iterator> match;
    std::regex re(R"(layout ?\(location ?= (\d+)?\) (\w+) \w+ (\w+))");

    const char* end = std::strchr(source, 0);

    while (std::regex_search(source, end, match, re)) {
      _locations[match[3].str()] = std::stoi(match[1].first);
      source = match[0].second;
    }
  };

  set_locations(vert.c_str());
  set_locations(frag.c_str());

  return prog;
}

void Graphics::_compile_vao() {
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);


  int stride = get_stride();
  size_t offset = 0;

  // Bind the VBO to binding index 0 of the VAO
  glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);

  for (auto [i, p]: enumerate(attribute_pointers)) {
        // Enable attribute i
        glEnableVertexArrayAttrib(vao, i);

        // Describe format of attribute i
        if (p.attribType == AttribType::Int) {
          glVertexArrayAttribIFormat(
              vao,
              i,
              p.size,
              p.type,
              (GLuint)offset
          );
        } else {
          glVertexArrayAttribFormat(
              vao,
              i,
              p.size,
              p.type,
              p.attribType == AttribType::FloatNorm,
              (GLuint)offset
          );
        }

        // Link attribute i to binding index 0
        glVertexArrayAttribBinding(vao, i, 0);
    
        auto s = p.type == GL_UNSIGNED_BYTE ? 1 : 4;
        offset += p.size * s;
  }

  glVertexArrayBindingDivisor(vao, 0, this->vertex_array_binding_divisor);
}

void Graphics::init_draw() {

  glUseProgram(program);

  for (auto& cb : draw_cbs) { cb(); }
  for (auto [slot, texture] : textures) {
    glUniform1i(loc(texture.name), slot);
    glBindTextureUnit(slot, texture.texture);
  }

  glBindVertexArray(vao);
}





void Graphics::uniform1i(const char* name, int value) const {
    glProgramUniform1i(program, glGetUniformLocation(program, name), value);
}
void Graphics::uniform1ui(const char* name, int value) const {
    glProgramUniform1ui(program, glGetUniformLocation(program, name), value);
}
void Graphics::uniform1f(const char* name, float value) const {
    glProgramUniform1f(program, glGetUniformLocation(program, name), value);
}
void Graphics::uniform2f(const char* name, float val0, float val1) const {
    glProgramUniform2f(program, glGetUniformLocation(program, name), val0, val1);
}
void Graphics::uniform3f(const char* name, float val0, float val1, float val2) const {
    glProgramUniform3f(program, glGetUniformLocation(program, name), val0, val1, val2);
}
void Graphics::uniform4f(const char* name, float val0, float val1, float val2, float val3) const {
    glProgramUniform4f(program, glGetUniformLocation(program, name), val0, val1, val2, val3);
}
void Graphics::uniform4f(const char* name, float vals[4]) const {
    glProgramUniform4f(program, glGetUniformLocation(program, name), vals[0], vals[1], vals[2], vals[3]);
}
void Graphics::uniform2fv(const char* name, size_t count, const float* data) const {
    glProgramUniform2fv(program, glGetUniformLocation(program, name), count, data);
}

void Graphics::uniform_matrix_3fv(const char* name, size_t count, bool normalize, const float* data) const {
    auto loc = glGetUniformLocation(program, name);
    glProgramUniformMatrix3fv(program, loc, count, normalize, data);
}







GLuint Graphics::_compile_program() const {

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
        "%s shader compilation failed in %s:\n%s\n\n", 
        (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment"),
        name.c_str(), infoLog
      );
      glDeleteShader(shader);
      return GLuint(0);
    }
    return shader;
  };

  GLuint vertexShader = compile_shader(GL_VERTEX_SHADER, vert.c_str());
  GLuint fragmentShader = compile_shader(GL_FRAGMENT_SHADER, frag.c_str());

  if (!vertexShader || !fragmentShader) {
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
    printf("Shader linking failed in %s:\n%s\n\n", name.c_str(), infoLog);
    glDeleteProgram(ShaderProgram);
    return GLuint(0);
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  return ShaderProgram;
}
