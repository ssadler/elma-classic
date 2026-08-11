
#include "main.h"

#include <cstring>
#include <exception>
#include <format>
#include <string>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <thread>
#include <fstream>
#include <sstream>
#include <regex>
#include <string_view>
#include <string>

#include "renderer/opengl.h"

using namespace std::chrono_literals;



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

  bool replaced = false;

  if (dirty != nullptr && dirty->exchange(false)) {
    auto p = _compile_shader();
    if (p <= 0) {
      printf("error recompiling program\n");
    } else {
      glUseProgram(p);
      auto old = program;
      program = p;
      glDeleteProgram(old);
      replaced = true;
    }
  }

  glUseProgram(program);

  for (auto& cb : draw_cbs) { cb(); }
  for (auto& [name, cb] : persistant_uniforms) { cb(loc(name)); }
  for (auto [slot, texture] : textures) {
    glUniform1i(loc(texture.name), slot);
    glBindTextureUnit(slot, texture.texture);
  }

  glBindVertexArray(vao);

  if (replaced) {
    //auto e = glGetError(); if (e != 0) { printf("%s:%i  e=%i\n", __FILE__, __LINE__, e); exit(1); }
  }
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

void Graphics::persist_uniform1i(const char* name, int value) {
  persist_uniform(name, [=](GLuint idx) { glUniform1i(idx, value); });
}
void Graphics::persist_uniform1f(const char* name, float value) {
  persist_uniform(name, [=](GLuint idx) { glUniform1f(idx, value); });
}
void Graphics::persist_uniform2f(const char* name, float val0, float val1) {
  persist_uniform(name, [=](GLuint idx) { glUniform2f(idx, val0, val1); });
}
void Graphics::persist_uniform2f(const char* name, vect2 vals){
  persist_uniform2f(name, vals.x, vals.y);
}
void Graphics::persist_uniform3f(const char* name, float val0, float val1, float val2) {
  persist_uniform(name, [=](GLuint idx) { glUniform3f(idx, val0, val1, val2); });
}
void Graphics::persist_uniform4f(const char* name, float val0, float val1, float val2, float val3) {
  persist_uniform(name, [=](GLuint idx) { glUniform4f(idx, val0, val1, val2, val3); });
}
void Graphics::persist_uniform4f(const char* name, float vals[4]) {
  persist_uniform(name, [=](GLuint idx) { glUniform4f(idx, vals[0], vals[1], vals[2], vals[3]); });
}






void Graphics::watch_file_shaders() {
  watch_file_shaders(name);
}


void Graphics::watch_file_shaders(std::string shadername) {

  if (!file_shader_name.empty()) {
    external_error("watch_file_shaders: shader name already set");
  }

  file_shader_name = shadername;

  auto shaders_dir = std::getenv("ELMA_SHADERS_DIR");

  if (shaders_dir == nullptr || shaders_dir[0] == '\0') {
    return;
  }

  auto watch_shader = [&](const char* type, std::string* target) {

    auto fname = std::format("{}.{}.glsl", shadername, type);
    auto path = std::format("{}/{}", shaders_dir, fname);

    if (!std::filesystem::exists(path)) {
      std::ofstream f(path);
      f << *target;
      f.close();
      printf("wrote shader: %s\n", path.c_str());
    }

    return [path, target, this, fname](std::stop_token stopToken) { // NOLINT

      printf("watching shader: %s\n", fname.c_str());

      std::filesystem::file_time_type last_write;

      while (!stopToken.stop_requested()) {
        try {
          auto current = std::filesystem::last_write_time(path);

          if (current != last_write) {

            std::ifstream f(path);
            std::ostringstream ss;
            ss << f.rdbuf();
            f.close();
            *target = ss.str();

            last_write = current;
            dirty->store(true, std::memory_order_release);
            printf("updated shader %s\n", fname.c_str());
          }
        } catch (std::exception& e) {
          printf("Error polling shader %s: %s\n", fname.c_str(), e.what());
          std::this_thread::sleep_for(20000ms);
        }

        std::this_thread::sleep_for(200ms);
      }

      printf("shader watcher exited: %s\n", fname.c_str());

    };
  };

  vert_watcher = std::make_shared<std::jthread>(watch_shader("vert", &vert));
  frag_watcher = std::make_shared<std::jthread>(watch_shader("frag", &frag));
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
