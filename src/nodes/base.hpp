#pragma once

#include <glad/glad.h>
#include <vector>
#include "../shader.hpp"
#include "../utils.hpp"

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
  std::vector<const char*> input_names; // list of input names
  std::vector<int> input_ids; // ids of input ops
  bool dirty = true; // whether the op needs to be re-evaluated
  FBO layer_fbo; // op result stored here

  // texture fields
  GLuint prog_id = 0;
  bool bypass = false;
  int out_w = 512;
  int out_h = 512;
  bool use_input_size = true;
  // TODO you have to resize to see the filter_mode update
  GLenum filter_mode = GL_NEAREST;

  // override output size to input size if set
  void apply_input_size(int input_w, int input_h) {
    if (use_input_size && input_w > 0 && input_h > 0) {
      out_w = input_w;
      out_h = input_h;
    }
  }

  // call this before applying the op
  void ensure_layer_fbo(int w, int h) {
    if (layer_fbo.tex.id == 0
      || layer_fbo.tex.w != w
      || layer_fbo.tex.h != h
    ) {
      if (layer_fbo.tex.id != 0) {
        glDeleteTextures(1, &layer_fbo.tex.id);
        glDeleteFramebuffers(1, &layer_fbo.fbo_id);
      }
      layer_fbo.create(w, h);
      layer_fbo.tex.set_filter_mode(filter_mode);
    }
  }

  virtual ~Op() = default;
  virtual char const* get_type_name() const = 0;
  virtual void apply(
    const std::vector<GLuint>&, /* input_textures */
    int /* input_w */,
    int /* input_h */
  ) {}
  // passes index or any unique id for ImGui element ids
  virtual void ui(int) {}
};