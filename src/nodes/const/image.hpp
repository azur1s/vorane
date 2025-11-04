#pragma once

#include "../base.hpp"

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
    use_input_size = false;
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

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(pixels);
    tex_w = w; tex_h = h;
    LOG_INFO("Loaded image: %s (%dx%d)", image_path.c_str(), w, h);
    want_reload = false;
  }

  void apply(const std::vector<GLuint>&, int, int) override {
    if (want_reload) {
      load_image();
      want_reload = false;
    }
    if (tex_id == 0) { return; }

    apply_input_size(tex_w, tex_h);
    ensure_layer_fbo(out_w, out_h);

    glBindFramebuffer(GL_FRAMEBUFFER, layer_fbo.fbo_id);
    glViewport(0, 0, out_w, out_h);
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