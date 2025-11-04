#pragma once

#include "../base.hpp"

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
    input_names = { "texture" };
    input_ids = { -1 };
  }

  void apply(const std::vector<GLuint>& input_textures, int input_w, int input_h) override {
    GLuint base_tex_id = 0;
    if (input_textures.empty()) { return; }
    base_tex_id = input_textures[0];

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