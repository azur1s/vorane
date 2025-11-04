#pragma once

#include "../base.hpp"

struct OpConstColor : public Op {
  float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  char const* get_type_name() const override { return "const/color"; }

  OpConstColor() {
    prog_id = make_fullscreen_program("shaders/const/color.frag");
    use_input_size = false;
  }

  void apply(const std::vector<GLuint>& input_textures, int, int) override {
    ensure_layer_fbo(out_w, out_h);

    glBindFramebuffer(GL_FRAMEBUFFER, layer_fbo.fbo_id);
    glUseProgram(prog_id);
    glUniform4fv(glGetUniformLocation(prog_id, "uColor"), 1, color);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  void ui(int i) override {
    ImGui::ColorPicker4(
      format_id("color", i),
      color,
      ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview
    );
  }
};