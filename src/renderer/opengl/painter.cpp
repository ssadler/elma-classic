
#include "main.h"

#include <cstring>
#include <string>
#include <string>

#include "renderer/opengl.h"
#include "renderer/opengl_gfx.h"





void GraphicsVAO::compile() {

    if (attribute_pointers.empty()) {
        internal_error("GraphicsVAO::compile: attribute pointers not set");
    }

    glBindVertexArray(vao.get());
    glBindBuffer(GL_ARRAY_BUFFER, vbo.get());

    int stride = get_stride();
    size_t offset = 0;

    for (int i=0; i<attribute_pointers.size(); i++) {
        auto& p = attribute_pointers[i];

        glEnableVertexAttribArray(i);

        // Describe format of attribute i
        if (p.attribType == AttribType::Int) {
            glVertexAttribIPointer(i, p.size, p.type, stride, (void*)offset);
        } else {
            auto norm = p.attribType == AttribType::FloatNorm;
            glVertexAttribPointer(i, p.size, p.type, norm, stride, (void*)offset);
        }

        // 1 when instanced otherwise 0
        glVertexAttribDivisor(i, this->vertex_array_binding_divisor);

        auto s = p.type == GL_UNSIGNED_BYTE ? 1 : 4;
        offset += p.size * s;
    }

}

void Graphics::init_draw() {

    program.use();

    for (auto [slot, texture] : textures) {
        glUniform1i(glGetUniformLocation(program.program.get(), texture.name), slot);
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, texture.texture);
    }

    vao.bind();
}





void GraphicsProgramCommon::uniform1i(const char* name, int value) const {
    glProgramUniform1i(prog(), glGetUniformLocation(prog(), name), value);
}
void GraphicsProgramCommon::uniform1ui(const char* name, int value) const {
    glProgramUniform1ui(prog(), glGetUniformLocation(prog(), name), value);
}
void GraphicsProgramCommon::uniform1f(const char* name, float value) const {
    glProgramUniform1f(prog(), glGetUniformLocation(prog(), name), value);
}
void GraphicsProgramCommon::uniform4i(const char* name, int val0, int val1, int val2, int val3) const {
    glProgramUniform4i(prog(), glGetUniformLocation(prog(), name), val0, val1, val2, val3);
}
void GraphicsProgramCommon::uniform2f(const char* name, float val0, float val1) const {
    glProgramUniform2f(prog(), glGetUniformLocation(prog(), name), val0, val1);
}
void GraphicsProgramCommon::uniform3f(const char* name, float val0, float val1, float val2) const {
    glProgramUniform3f(prog(), glGetUniformLocation(prog(), name), val0, val1, val2);
}
void GraphicsProgramCommon::uniform4f(const char* name, float val0, float val1, float val2, float val3) const {
    glProgramUniform4f(prog(), glGetUniformLocation(prog(), name), val0, val1, val2, val3);
}
void GraphicsProgramCommon::uniform4f(const char* name, float vals[4]) const {
    glProgramUniform4f(prog(), glGetUniformLocation(prog(), name), vals[0], vals[1], vals[2], vals[3]);
}
void GraphicsProgramCommon::uniform2fv(const char* name, size_t count, const float* data) const {
    glProgramUniform2fv(prog(), glGetUniformLocation(prog(), name), count, data);
}

void GraphicsProgramCommon::uniform_matrix_3fv(
        const char* name, size_t count, bool normalize, const float* data) const {
    auto loc = glGetUniformLocation(prog(), name);
    glProgramUniformMatrix3fv(prog(), loc, count, normalize, data);
}







void GraphicsProgram::compile() {

    GLint success;
    glGetProgramiv(program.get(), GL_LINK_STATUS, &success);
    if (success) {
        internal_error("Graphics::compile: already compiled");
    }

    if (frag.empty()) {
        internal_error("Graphics::compile: frag empty");
    }
    if (vert.empty()) {
        internal_error("Graphics::compile: vert empty");
    }

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
            exit(1);
        }
        return shader;
    };

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vert.c_str());
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, frag.c_str());

    glAttachShader(program.get(), vertex_shader);
    glAttachShader(program.get(), fragment_shader);
    glLinkProgram(program.get());

    glGetProgramiv(program.get(), GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program.get(), 512, nullptr, infoLog);
        printf("Shader linking failed in %s:\n%s\n\n", name.c_str(), infoLog);
        glDeleteProgram(program.get());
        exit(1);
    }
}

