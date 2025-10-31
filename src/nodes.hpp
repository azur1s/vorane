#pragma once

#include <glad/glad.h>
#include <vector>
#include "shader.hpp"
#include "utils.hpp"

#define format_id(str, id) std::format("{}##{}", str, id).c_str()

enum class MixType {
  MixNormal = 0,
  MixMultiply,
  MixScreen,
  MixOverlay,
  MixSoftLight,
  MixHardLight,
  MixColorDodge,
  MixColorBurn,
  MixLinearDodge,
  MixLinearBurn,
  MixLighten,
  MixDarken,
  MixDifference,
  MixExclusion
};

struct Op {
  // graph-related fields
  int id = -1; // assigned by state
  std::vector<int> input_ids; // ids of input ops
  bool dirty = true; // whether the op needs to be re-evaluated
  FBO layer_fbo; // op result stored here

  GLuint prog_id = 0;
  bool bypass = false;

  // call this before applying the op
  void ensure_layer_fbo(int w, int h) {
    if (layer_fbo.tex.id == 0 || layer_fbo.tex.w != w || layer_fbo.tex.h != h) {
      if (layer_fbo.tex.id != 0) {
        glDeleteTextures(1, &layer_fbo.tex.id);
        glDeleteFramebuffers(1, &layer_fbo.fbo_id);
      }
      layer_fbo.tex.createRGBA8(w, h);
      layer_fbo.create(w, h);
    }
  }

  virtual ~Op() = default;
  virtual char const* get_type_name() const = 0;
  virtual void apply(
    const std::vector<GLuint>& /* input_textures */,
    int /* out_w */,
    int /* out_h */
  ) {}
  // passes index or any unique id for ImGui element ids
  virtual void ui(int) {}
};

struct OpConstColor : public Op {
  float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  char const* get_type_name() const override { return "const/color"; }

  OpConstColor() {
    prog_id = make_fullscreen_program("shaders/const/color.frag");
  }

  void apply(const std::vector<GLuint>&, int out_w, int out_h) override {
    ensure_layer_fbo(out_w, out_h);

    glBindFramebuffer(GL_FRAMEBUFFER, layer_fbo.fbo_id);
    glUseProgram(prog_id);
    glUniform4fv(glGetUniformLocation(prog_id, "uColor"), 1, color);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  void ui(int i) override {
    ImGui::ColorEdit4(
      format_id("color", i),
      color
    );
  }
};

// TODO cache by path at state to avoid loading the same image multiple times
struct OpConstImage : public Op {
  std::string image_path;
  GLuint tex_id = 0;
  int tex_w = 0, tex_h = 0;
  bool want_reload = false;
  char const* get_type_name() const override { return "const/image"; }

  OpConstImage(std::string path) : image_path(std::move(path)) {
    prog_id = make_fullscreen_program("shaders/const/image.frag");
    if (!path.empty()) load_image();
  }

  ~OpConstImage() override {
    if (tex_id) { glDeleteTextures(1, &tex_id); }
  }

  void load_image() {
    int w, h, n;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc* pixels = stbi_load(image_path.c_str(), &w, &h, &n, 4);
    if (!pixels) { 
      LOG_ERROR("Failed to load image: %s", image_path.c_str());
      return; 
    }

    if (!tex_id) glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(pixels);
    tex_w = w; tex_h = h;
    LOG_INFO("Loaded image: %s (%dx%d)", image_path.c_str(), w, h);
    want_reload = false;
  }

  void apply(const std::vector<GLuint>&, int out_w, int out_h) override {
    if (want_reload) {
      load_image();
      want_reload = false;
    }
    if (tex_id == 0) { return; }

    ensure_layer_fbo(out_w, out_h);

    glBindFramebuffer(GL_FRAMEBUFFER, layer_fbo.fbo_id);
    glUseProgram(prog_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glUniform1i(glGetUniformLocation(prog_id, "uTex"), 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  void ui(int i) override {
    char buffer[256];
    strncpy(buffer, image_path.c_str(), sizeof(buffer));
    if (ImGui::InputText(
          format_id("image path", i),
          buffer,
          sizeof(buffer))) {
      image_path = std::string(buffer);
    }
    if (ImGui::Button(format_id("reload", i))) {
      want_reload = true;
    }
  }
};

struct OpGenComposite : public Op {
  MixType mix_type = MixType::MixNormal;
  float opacity = 1.0f;
  char const* get_type_name() const override { return "gen/composite"; }

  OpGenComposite() {
    prog_id = make_fullscreen_program("shaders/gen/composite.frag");
  }

  void apply(const std::vector<GLuint>& input_textures, int out_w, int out_h) override {
    if (input_textures.size() < 2) { return; }
    GLuint base_tex_id  = input_textures[0];
    GLuint layer_tex_id = input_textures[1];

    ensure_layer_fbo(out_w, out_h);

    glBindFramebuffer(GL_FRAMEBUFFER, layer_fbo.fbo_id);
    glViewport(0, 0, out_w, out_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog_id);
    // texture 0: base
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, base_tex_id);
    glUniform1i(glGetUniformLocation(prog_id, "uBase"), 0);
    // texture 1: layer
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, layer_tex_id);
    glUniform1i(glGetUniformLocation(prog_id, "uLayer"), 1);
    glUniform1i(glGetUniformLocation(prog_id, "uMode"), static_cast<int>(mix_type));
    glUniform1f(glGetUniformLocation(prog_id, "uOpacity"), opacity);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  void ui(int i) override {
    const char *items[] = {
      "normal",
      "multiply",
      "screen",
      "overlay",
      "soft light",
      "hard light",
      "color dodge",
      "color burn",
      "linear dodge",
      "linear burn",
      "lighten",
      "darken",
      "difference",
      "exclusion"
    };
    int current_mix_type = static_cast<int>(mix_type);
    if (ImGui::Combo(
          format_id("mix type", i),
          &current_mix_type,
          items,
          IM_ARRAYSIZE(items))) {
      mix_type = static_cast<MixType>(current_mix_type);
    }
    ImGui::SliderFloat(
      format_id("opacity", i),
      &opacity,
      0.0f,
      1.0f
    );
  }
};

struct OpGenTransform : public Op {
  float offset_x        = 0.0f;
  float offset_y        = 0.0f;
  float size_x          = 1.0f;
  float size_y          = 1.0f;
  bool  size_uniform    = true;
  float angle           = 0.0f;
  bool  flip_horizontal = false;
  bool  flip_vertical   = false;
  char const* get_type_name() const override { return "gen/transform"; }

  OpGenTransform() {
    prog_id = make_fullscreen_program("shaders/gen/transform.frag");
  }

  void apply(const std::vector<GLuint>& input_textures, int out_w, int out_h) override {
    GLuint base_tex_id = 0;
    if (input_textures.empty()) { return; }
    base_tex_id = input_textures[0];

    ensure_layer_fbo(out_w, out_h);

    glBindFramebuffer(GL_FRAMEBUFFER, layer_fbo.fbo_id);
    glViewport(0, 0, out_w, out_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, base_tex_id);
    glUniform1i(glGetUniformLocation(prog_id, "uTex"), 0);

    AffineMat3 M = amat3_identity();
    ImVec2 pivot = {0.5f, 0.5f};

    // 1) move to pivot
    // 2) apply flip, scale, rotate
    // 3) move back
    // 4) finally apply UV translation (offset)
    M = amat3_mul(M, amat3_transform(offset_x / out_w, offset_y / out_h));
    M = amat3_mul(M, amat3_transform(+pivot.x, +pivot.y));
    M = amat3_mul(M, amat3_rotate(angle));
    M = amat3_mul(M, amat3_scale(
      size_uniform ? size_x : size_x,
      size_uniform ? size_x : size_y
    ));
    M = amat3_mul(M, amat3_scale(
      flip_horizontal ? -1.f : 1.f,
      flip_vertical   ? -1.f : 1.f
    ));
    M = amat3_mul(M, amat3_transform(-pivot.x, -pivot.y));

    glUniformMatrix3fv(glGetUniformLocation(prog_id, "uXform"), 1, GL_TRUE, M.m);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  void ui(int i) override {
    ImGui::SliderFloat(
      format_id("offset x", i),
      &offset_x,
      -1000.0f,
      1000.0f
    );
    ImGui::SliderFloat(
      format_id("offset y", i),
      &offset_y,
      -1000.0f,
      1000.0f
    );
    ImGui::SliderFloat(
      format_id("size x", i),
      &size_x,
      0.01f,
      10.0f
    );
    ImGui::SliderFloat(
      format_id("size y", i),
      size_uniform ? &size_x : &size_y,
      0.01f,
      10.0f
    );
    ImGui::Checkbox(
      format_id("size uniform", i),
      &size_uniform
    );
    ImGui::SliderFloat(
      format_id("angle", i),
      &angle,
      0.0f,
      360.0f
    );
    ImGui::Checkbox(
      format_id("flip horizontal", i),
      &flip_horizontal
    );
    ImGui::Checkbox(
      format_id("flip vertical", i),
      &flip_vertical
    );
  }
};
