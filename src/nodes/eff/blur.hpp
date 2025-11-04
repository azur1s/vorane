#pragma once

#include "../base.hpp"

struct OpEffBlur : public Op {
  FBO temp_fbo;
  GLuint prog_v_id = 0; // use prog_id for horizontal pass
  float radius_x = 5.0f;
  float radius_y = 5.0f;
  bool radius_uniform = true;
  char const* get_type_name() const override { return "eff/blur"; }

  OpEffBlur() {
    prog_id   = make_fullscreen_program("shaders/eff/gaussian_h.frag");
    prog_v_id = make_fullscreen_program("shaders/eff/gaussian_v.frag");
    input_names = { "texture" };
    input_ids = { -1 };
  }

  void apply(const std::vector<GLuint>& input_textures, int input_w, int input_h) override {
    if (input_textures.empty()) { return; }
    GLuint base_tex_id = input_textures[0];

    apply_input_size(input_w, input_h);
    ensure_layer_fbo(out_w, out_h);

    // horizontal pass
    if (temp_fbo.tex.id == 0 || temp_fbo.tex.w != out_w || temp_fbo.tex.h != out_h) {
      if (temp_fbo.tex.id != 0) {
        glDeleteTextures(1, &temp_fbo.tex.id);
        glDeleteFramebuffers(1, &temp_fbo.fbo_id);
      }
      temp_fbo.create(out_w, out_h);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, temp_fbo.fbo_id);
    glViewport(0, 0, out_w, out_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, base_tex_id);
    glUniform1i(glGetUniformLocation(prog_id, "uTex"), 0);
    glUniform1f(glGetUniformLocation(prog_id, "uRadius"), radius_uniform ? radius_x : radius_x);
    glUniform2f(glGetUniformLocation(prog_id, "uTexelSize"), 1.0f / out_w, 1.0f / out_h);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // vertical pass
    glBindFramebuffer(GL_FRAMEBUFFER, layer_fbo.fbo_id);
    glViewport(0, 0, out_w, out_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog_v_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, temp_fbo.tex.id);
    glUniform1i(glGetUniformLocation(prog_v_id, "uTex"), 0);
    glUniform1f(glGetUniformLocation(prog_v_id, "uRadius"), radius_uniform ? radius_x : radius_y);
    glUniform2f(glGetUniformLocation(prog_v_id, "uTexelSize"), 1.0f / out_w, 1.0f / out_h);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  void ui(int i) override {
    ImGui::SliderFloat(
      format_id("radius x", i),
      &radius_x,
      0.0f,
      100.0f
    );
    ImGui::SliderFloat(
      format_id("radius y", i),
      radius_uniform ? &radius_x : &radius_y,
      0.0f,
      100.0f
    );
    ImGui::Checkbox(
      format_id("radius uniform", i),
      &radius_uniform
    );
  }
};