#pragma once

#include <glad/glad.h>
#include "utils.hpp"

static void glCheck(bool cond, const char* msg) {
  if (!cond) { throw std::runtime_error(msg); }
}

static GLuint compile_shader(GLenum type, const char* src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);
  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char log[1024];
    glGetShaderInfoLog(shader, 1024, NULL, log);
    LOG_ERROR("Shader compilation error\n%s", log);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader) {
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char log[1024];
    glGetProgramInfoLog(program, 1024, NULL, log);
    LOG_ERROR("Program linking error: %s", log);
    glDeleteProgram(program);
    return 0;
  }
  return program;
}

static std::string load_source(const char* filepath) {
  FILE* file = fopen(filepath, "rb");
  glCheck(file != nullptr, "Failed to open shader file");
  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  fseek(file, 0, SEEK_SET);
  std::string source(size, '\0');
  fread(source.data(), 1, size, file);
  fclose(file);
  return source;
}

struct Texture {
  GLuint id = 0;
  int w = 0, h = 0;
  void createRGBA8(int W, int H, const void* pixels = nullptr){
    w = W; h = H;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
};

struct FBO {
  GLuint fbo_id = 0;
  Texture tex;
  // TODO handle texture creation inside create() so we dont have to do fbo.tex.*
  void create(int width, int height) {
    glGenFramebuffers(1, &fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.id, 0);
    glCheck(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void resize(int width, int height) {
    if (tex.id != 0) glDeleteTextures(1, &tex.id);
    tex.createRGBA8(width, height);
    if (fbo_id != 0) glDeleteFramebuffers(1, &fbo_id);
    glGenFramebuffers(1, &fbo_id);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.id, 0);
    glCheck(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO incomplete after resize");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
};

static GLuint make_fullscreen_program(const char* fragment_path) {
  std::string vertex_src   = load_source("./shaders/fullscreen.vert");
  std::string fragment_src = load_source(fragment_path);
  GLuint vertex_shader   = compile_shader(GL_VERTEX_SHADER, vertex_src.c_str());
  GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_src.c_str());
  if (!vertex_shader || !fragment_shader) {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    LOG_ERROR("Failed to compile shaders for %s", fragment_path);
    return 0;
  } else {
    GLuint program = link_program(vertex_shader, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
  }
}
