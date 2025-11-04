#pragma once

#include "../base.hpp"

struct OpGenGrade : public Op {
  float lift   = 0.0f; // [-1, 1]
  float gamma  = 1.0f; // [0.01, 3]
  float gain   = 1.0f; // [0, 4]
  float offset = 0.0f; // [-1, 1]
  float strength = 1.0f; // or mix factor
  char const* get_type_name() const override { return "gen/grade"; }

  OpGenGrade() {
    prog_id = make_fullscreen_program("shaders/gen/grade.frag");
    input_names = { "texture" };
    input_ids = { -1 };
  }

  void apply(const std::vector<GLuint>& input_textures, int input_w, int input_h) override {
    if (input_textures.empty()) { return; }
    GLuint base_tex_id = input_textures[0];

    apply_input_size(input_w, input_h);
    ensure_layer_fbo(out_w, out_h);

    glBindFramebuffer(GL_FRAMEBUFFER, layer_fbo.fbo_id);
    glViewport(0, 0, out_w, out_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(prog_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, base_tex_id);
    glUniform1i(glGetUniformLocation(prog_id, "uTex"), 0);
    glUniform3f(glGetUniformLocation(prog_id, "uLift"),   lift,   lift,   lift);
    glUniform3f(glGetUniformLocation(prog_id, "uGamma"),  gamma,  gamma,  gamma);
    glUniform3f(glGetUniformLocation(prog_id, "uGain"),   gain,   gain,   gain);
    glUniform3f(glGetUniformLocation(prog_id, "uOffset"), offset, offset, offset);
    glUniform1f(glGetUniformLocation(prog_id, "uStrength"), strength);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  void ui(int i) override {
    ImGui::SliderFloat(
      format_id("lift", i),
      &lift,
      -1.0f,
      1.0f
    );
    ImGui::SliderFloat(
      format_id("gamma", i),
      &gamma,
      0.01f,
      3.0f
    );
    ImGui::SliderFloat(
      format_id("gain", i),
      &gain,
      0.0f,
      4.0f
    );
    ImGui::SliderFloat(
      format_id("offset", i),
      &offset,
      -1.0f,
      1.0f
    );
    ImGui::SliderFloat(
      format_id("strength", i),
      &strength,
      0.0f,
      1.0f
    );
  }
};