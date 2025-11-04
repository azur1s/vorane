#pragma once

#include "../base.hpp"

struct OpEffDither : public Op {
  float steps = 8.0f;
  float scale = 1.0f;
  char const* get_type_name() const override { return "eff/dither"; }

  OpEffDither() {
    prog_id = make_fullscreen_program("shaders/eff/dither.frag");
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
    glUniform2f(glGetUniformLocation(prog_id, "uViewportSize"), (float)out_w, (float)out_h);
    glUniform1f(glGetUniformLocation(prog_id, "uSteps"), steps);
    glUniform1f(glGetUniformLocation(prog_id, "uDitherScale"), scale);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  void ui(int i) override {
    ImGui::SliderFloat(
      format_id("steps", i),
      &steps,
      1.0f,
      256.0f
    );
    ImGui::SliderFloat(
      format_id("scale", i),
      &scale,
      0.01f,
      10.0f
    );
  }
};