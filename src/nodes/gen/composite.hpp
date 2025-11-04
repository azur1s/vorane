#pragma once

#include "../base.hpp"

struct OpGenComposite : public Op {
  MixType mix_type = MixType::MixNormal;
  float opacity = 1.0f;
  char const* get_type_name() const override { return "gen/composite"; }

  OpGenComposite() {
    prog_id = make_fullscreen_program("shaders/gen/composite.frag");
    input_names = { "base texture", "layer texture" };
    input_ids = { -1, -1 };
  }

  void apply(const std::vector<GLuint>& input_textures, int input_w, int input_h) override {
    if (input_textures.size() < 2) { return; }
    GLuint base_tex_id  = input_textures[0];
    GLuint layer_tex_id = input_textures[1];

    apply_input_size(input_w, input_h);
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