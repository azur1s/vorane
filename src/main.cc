#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if defined(_WIN32)
#include <windows.h>
extern "C" {
    #if defined(ASK_FOR_HIGH_PERFORMANCE_GPU)
    // For NVIDIA
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    // For AMD
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
    #else
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000000;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0;
    #endif
}
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <format>
#include <memory>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "style.h"
#include "utils.h"

static void gl_check(bool cond, const char* msg) {
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
  gl_check(file != nullptr, "Failed to open shader file");
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
  void create(int width, int height) {
    glGenFramebuffers(1, &fbo_id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.id, 0);
    gl_check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "FBO incomplete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
};

enum class MixType {
  MixNormal,
  MixAdd,
  MixMultiply,
  MixOverlay,
};

struct Op {
  GLuint prog_id = 0;
  bool bypass = false;
  MixType mix_type = MixType::MixNormal;
  float opacity = 1.0f;
  virtual ~Op() = default;
  virtual char const* get_type_name() const = 0;
  virtual void apply(GLuint tex_id, int out_w, int out_h) {}
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

struct OpConstColor : public Op {
  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  char const* get_type_name() const override { return "const/color"; }

  OpConstColor() {
    prog_id = make_fullscreen_program("shaders/const_color.frag");
  }

  void apply(GLuint tex_id, int out_w, int out_h) override {
    glUseProgram(prog_id);
    glUniform4fv(glGetUniformLocation(prog_id, "uColor"), 1, color);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
};

struct OpConstGradient : public Op {
  float color_a[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float color_b[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  float angle = 0.0f;
  char const* get_type_name() const override { return "const/gradient"; }

  void apply(GLuint tex_id, int out_w, int out_h) override {
    // TODO
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
    prog_id = make_fullscreen_program("shaders/gen_transform.frag");
  }

  void apply(GLuint tex_id, int out_w, int out_h) override {
    glUseProgram(prog_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_id);
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
};

struct OpEffBlur : public Op {
  float radius = 5.0f;
  char const* get_type_name() const override { return "eff/blur"; }

  void apply(GLuint tex_id, int out_w, int out_h) override {
    // TODO
  }
};

#define INIT_FLOAT4(v, a, b, c, d) \
  v[0] = a; \
  v[1] = b; \
  v[2] = c; \
  v[3] = d;

struct state_t {
  int canvas_w = 0; // init later
  int canvas_h = 0;
  FBO ping, pong;
  std::vector<std::unique_ptr<Op>> ops;

  float zoom_factor  = 1.0f;
  float last_mouse_x = 0.0f;
  float last_mouse_y = 0.0f;
  float pan_x        = 0.0f;
  float pan_y        = 0.0f;
  bool isfullscreen   = false;
  bool isopen_editor  = true;
  bool isconfirm_exit = false;

  GLuint run(GLuint input_tex_id) {
    bool usePing = true;
    GLuint current = input_tex_id;
    for(size_t i = 0; i < ops.size(); ++i) {
      if (ops[i]->bypass) continue;
      // dont run if op is invalid
      if (ops[i]->prog_id == 0) continue;
      FBO &dst = usePing ? ping : pong;
      glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo_id);
      glViewport(0, 0, canvas_w, canvas_h);
      ops[i]->apply(current, canvas_w, canvas_h);
      current = dst.tex.id;
      usePing = !usePing;
    }
    return current;
  }
};
static state_t g_state;

void window_close_callback(GLFWwindow *window) {
  if (!g_state.isconfirm_exit) {
    g_state.isconfirm_exit = true;
  }
  glfwSetWindowShouldClose(window, GLFW_FALSE);
  return;
}

#define format_id(str, id) std::format("{}##{}", str, id).c_str()

static void glfwErrorCallback(int code, const char* desc) {
  LOG_ERROR("GLFW Error (%d): %s", code, desc);
}

int main() {
  glfwSetErrorCallback(glfwErrorCallback);
  if (!glfwInit()) {
    LOG_ERROR("Failed to initialize GLFW");
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(800, 600, "vorane", NULL, NULL);
  if (!window) {
    LOG_ERROR("Failed to create GLFW window");
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    LOG_ERROR("Failed to initialize GLAD");
    return -1;
  }

  LOG_INFO("vorane said hello");
  LOG_INFO("OpenGL : %s", glGetString(GL_VERSION));
  LOG_INFO("GLSL   : %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
  LOG_INFO("Vendor : %s", glGetString(GL_VENDOR));

  glfwMaximizeWindow(window);
  glfwSwapInterval(1); // Enable vsync
  glfwSetWindowCloseCallback(window, window_close_callback);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO(); (void)io;
  SetupImGuiStyle();
  io.IniFilename = NULL;
  // ImGui::LoadIniSettingsFromDisk("./imgui.ini");
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.Fonts->AddFontFromFileTTF("./assets/geistmono.ttf", 14.0f);

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // load image
  int iw, ih, nc;
  stbi_set_flip_vertically_on_load(1);
  stbi_uc *image_data = stbi_load("./test.jpg", &iw, &ih, &nc, 4);
  gl_check(image_data != nullptr, "Failed to load image");
  g_state.canvas_w = iw;
  g_state.canvas_h = ih;

  Texture input_texture;
  input_texture.createRGBA8(iw, ih, image_data);
  stbi_image_free(image_data);

  // init vao & fbos
  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  g_state.ping.tex.createRGBA8(g_state.canvas_w, g_state.canvas_h);
  g_state.ping.create(g_state.canvas_w, g_state.canvas_h);
  g_state.pong.tex.createRGBA8(g_state.canvas_w, g_state.canvas_h);
  g_state.pong.create(g_state.canvas_w, g_state.canvas_h);

  GLuint display_prog = make_fullscreen_program("shaders/present.frag");

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);

    // --- inputs

    if (io.MouseWheel != 0.0f && !io.WantCaptureMouse) {
      float zoom_step = 0.1f;
      if (io.KeyCtrl) {
        zoom_step = 0.01f;
      }
      if (io.MouseWheel > 0.0f) {
        g_state.zoom_factor *= (1.0f + zoom_step);
      } else {
        g_state.zoom_factor /= (1.0f + zoom_step);
      }
    }

    if (io.MouseDown[0] && !io.WantCaptureMouse) {
      float dx = io.MousePos.x - g_state.last_mouse_x;
      float dy = io.MousePos.y - g_state.last_mouse_y;
      g_state.pan_x += dx * 0.5f;
      g_state.pan_y += dy * 0.5f;
    }
    g_state.last_mouse_x = io.MousePos.x;
    g_state.last_mouse_y = io.MousePos.y;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W)) {
      g_state.isconfirm_exit = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
      g_state.isopen_editor = !g_state.isopen_editor;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
      g_state.isfullscreen = !g_state.isfullscreen;
      if (g_state.isfullscreen) {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                             mode->refreshRate);
      } else {
        glfwSetWindowMonitor(window, NULL, 100, 100, 800, 600, 0);
      }
    }

    // --- render

    GLuint final_tex = g_state.run(input_texture.id);

    // scale to fit window instead of stretch
    float aspect_canvas = (float)g_state.canvas_w / (float)g_state.canvas_h;
    float aspect_window = (float)io.DisplaySize.x / (float)io.DisplaySize.y;

    float wn = 1.0f, hn = 1.0f;
    if (aspect_window > aspect_canvas) {
      // full height; reduced width
      wn = aspect_canvas / aspect_window;
    } else {
      // full width; reduced height
      hn = aspect_window / aspect_canvas;
    }

    float offx = (1.0 - wn) * 0.5f + (g_state.pan_x / (float)io.DisplaySize.x) * 2.0f;
    float offy = (1.0 - hn) * 0.5f - (g_state.pan_y / (float)io.DisplaySize.y) * 2.0f;
    float zoom = g_state.zoom_factor;

    float sx = wn * zoom;
    float sy = hn * zoom;
    AffineMat3 M = amat3_identity();
    M = amat3_mul(M, amat3_transform(-offx, -offy));
    M = amat3_mul(M, amat3_scale(1.0f/sx, 1.0f/sy));

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, display_w, display_h);
    glUseProgram(display_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, final_tex);
    glUniform1i(glGetUniformLocation(display_prog, "uTex"), 0);
    glUniformMatrix3fv(glGetUniformLocation(display_prog, "uXform"), 1, GL_TRUE, M.m);
    glUniform2f(glGetUniformLocation(display_prog, "uCanvasSize"), (float)g_state.canvas_w, (float)g_state.canvas_h);
    glUniform1f(glGetUniformLocation(display_prog, "uCheckerSize"), 32.0f * zoom);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // --- ui

    // set font
    ImGui::PushFont(io.Fonts->Fonts[0]);

    // menu bar
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("file")) {
        if (ImGui::MenuItem("exit", "C-W")) {
          g_state.isconfirm_exit = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("view")) {
        if (ImGui::MenuItem("editor", "E", g_state.isopen_editor)) {
          g_state.isopen_editor = !g_state.isopen_editor;
        }
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }

    if (g_state.isconfirm_exit) {
      ImGui::OpenPopup("exit");
    }

    if (ImGui::BeginPopupModal("exit", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("are you sure you want to exit?");
      ImGui::Separator();
      if (ImGui::Button("yes", ImVec2(100, 0))) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
      }
      ImGui::SetItemDefaultFocus();
      ImGui::SameLine();
      if (ImGui::Button("no", ImVec2(100, 0))) {
        ImGui::CloseCurrentPopup();
        g_state.isconfirm_exit = false;
      }
      ImGui::EndPopup();
    }

    // editor
    float margin = 10.0f;
    if (g_state.isopen_editor) {
      float window_width = io.DisplaySize.x * 0.25f;
      ImGui::SetNextWindowSize(
        ImVec2(window_width, io.DisplaySize.y - 20.0f - (margin * 2.0f)),
        ImGuiCond_FirstUseEver
      );
      ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x - window_width - margin, 20.0f + margin),
        ImGuiCond_FirstUseEver
      );

      ImGui::Begin("editor", &g_state.isopen_editor);

      if (ImGui::BeginCombo("add operation", "...")) {
        #define CHECK_PROG_ID_AND_PUSH(op) \
          if (!op.prog_id) { \
            LOG_ERROR("Failed to create shader for %s", op.get_type_name()); \
          } else { \
            g_state.ops.push_back(std::make_unique<decltype(op)>(op)); \
          }

        if (ImGui::Selectable("const/color")) {
          OpConstColor o;
          INIT_FLOAT4(o.color, 1.0f, 1.0f, 1.0f, 1.0f);
          CHECK_PROG_ID_AND_PUSH(o);
        }
        if (ImGui::Selectable("const/gradient")) {
          OpConstGradient o;
          INIT_FLOAT4(o.color_a, 1.0f, 1.0f, 1.0f, 1.0f);
          INIT_FLOAT4(o.color_b, 0.0f, 0.0f, 0.0f, 1.0f);
          o.angle = 0.0f;
          CHECK_PROG_ID_AND_PUSH(o);
        }
        if (ImGui::Selectable("gen/transform")) {
          OpGenTransform o;
          o.offset_x        = 0.0f;
          o.offset_y        = 0.0f;
          o.size_x          = 1.0f;
          o.size_y          = 1.0f;
          o.size_uniform    = true;
          o.angle           = 0.0f;
          o.flip_horizontal = false;
          o.flip_vertical   = false;
          CHECK_PROG_ID_AND_PUSH(o);
        }
        if (ImGui::Selectable("eff/blur")) {
          OpEffBlur o;
          o.radius = 5.0f;
          CHECK_PROG_ID_AND_PUSH(o);
        }
        ImGui::EndCombo();
      }

      ImGui::Separator();

      ImGui::Text("operations:");

      for (size_t i = 0; i < g_state.ops.size(); i++) {
        Op &operation = *(g_state.ops[i]);
        const char *type_name = operation.get_type_name();

        const char *header = std::format("{} - {}", i, type_name).c_str();

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        if (ImGui::CollapsingHeader(header)) {
          ImGui::Indent();
          if (ImGui::Button(format_id("bypass", i))) {
            g_state.ops[i]->bypass = !g_state.ops[i]->bypass;
          }
          ImGui::SameLine();
          if (ImGui::Button(format_id("remove", i))) {
            g_state.ops.erase(g_state.ops.begin() + i);
            i--;
            continue;
          }
          ImGui::SameLine();
          if (ImGui::Button(format_id("move up", i)) && i > 0) {
            std::swap(g_state.ops[i], g_state.ops[i - 1]);
            continue;
          }
          ImGui::SameLine();
          if (ImGui::Button(format_id("move down", i)) && i + 1 < g_state.ops.size()) {
            std::swap(g_state.ops[i], g_state.ops[i + 1]);
            continue;
          }

          ImGui::Spacing();

          ImGui::SetNextItemOpen(true, ImGuiCond_Once);
          if (ImGui::CollapsingHeader(format_id("settings", i))) {
            if (strcmp(type_name, "const/color") == 0) {
              ImGui::ColorEdit4(
                format_id("color", i),
                (float *)&static_cast<OpConstColor &>(operation).color
              );
            } else if (strcmp(type_name, "const/gradient") == 0) {
              OpConstGradient &opg = static_cast<OpConstGradient &>(operation);
              ImGui::ColorEdit4(
                format_id("color A", i),
                (float *)&opg.color_a
              );
              ImGui::ColorEdit4(
                format_id("color B", i),
                (float *)&opg.color_b
              );
              ImGui::SliderFloat(
                format_id("angle", i),
                &opg.angle,
                0.0f,
                360.0f
              );
            } else if (strcmp(type_name, "gen/transform") == 0) {
              OpGenTransform &opt = static_cast<OpGenTransform &>(operation);
              ImGui::SliderFloat(
                format_id("offset x", i),
                &opt.offset_x,
                -1000.0f,
                1000.0f
              );
              ImGui::SliderFloat(
                format_id("offset y", i),
                &opt.offset_y,
                -1000.0f,
                1000.0f
              );
              ImGui::SliderFloat(
                format_id("size x", i),
                &opt.size_x,
                0.01f,
                10.0f
              );
              ImGui::SliderFloat(
                format_id("size y", i),
                opt.size_uniform ? &opt.size_x : &opt.size_y,
                0.01f,
                10.0f
              );
              ImGui::Checkbox(
                format_id("size uniform", i),
                &opt.size_uniform
              );
              ImGui::SliderFloat(
                format_id("angle", i),
                &opt.angle,
                0.0f,
                360.0f
              );
              ImGui::Checkbox(
                format_id("flip horizontal", i),
                &opt.flip_horizontal
              );
              ImGui::Checkbox(
                format_id("flip vertical", i),
                &opt.flip_vertical
              );
            } else if (strcmp(type_name, "eff/blur") == 0) {
              OpEffBlur &ope = static_cast<OpEffBlur &>(operation);
              ImGui::SliderFloat(
                format_id("radius", i),
                &ope.radius,
                0.0f,
                25.0f
              );
            }
          }

          ImGui::Spacing();

          if (ImGui::CollapsingHeader(format_id("blending mode", i))) {
            const char *items[] = {
              "normal",
              "add",
              "multiply",
              "overlay"
            };

            int current_item = 0;

            switch (operation.mix_type) {
            case MixType::MixNormal:   current_item = 0; break;
            case MixType::MixAdd:      current_item = 1; break;
            case MixType::MixMultiply: current_item = 2; break;
            case MixType::MixOverlay:  current_item = 3; break;
            }

            if (ImGui::Combo(
                  format_id("type", i),
                  &current_item,
                  items,
                  IM_ARRAYSIZE(items))) {
              switch (current_item) {
              case 0: operation.mix_type = MixType::MixNormal;   break;
              case 1: operation.mix_type = MixType::MixAdd;      break;
              case 2: operation.mix_type = MixType::MixMultiply; break;
              case 3: operation.mix_type = MixType::MixOverlay;  break;
              }
            }

            ImGui::SliderFloat(
              format_id("opacity", i),
              &operation.opacity,
              0.0f,
              1.0f
            );
          }
          ImGui::Unindent();
        }
      }

      ImGui::End();

      // profiler window
      ImGui::SetNextWindowSize(
        ImVec2(200.0f, 100.0f),
        ImGuiCond_FirstUseEver
      );
      ImGui::SetNextWindowPos(
        ImVec2(margin, 20.0f + margin),
        ImGuiCond_FirstUseEver
      );
      GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
      ImGui::Begin("profiler", nullptr, ImGuiWindowFlags_NoCollapse);
      ImGui::Text("viewport: (%d, %d) %d x %d", vp[0], vp[1], vp[2], vp[3]);
      ImGui::Text("canvas: %d x %d", g_state.canvas_w, g_state.canvas_h);
      ImGui::Text("mem: %s", format_bytes(get_mem_usage()).c_str());
      ImGui::Text("fps: %.1f", io.Framerate);
      ImGui::Text("zoom: %.2f%%", g_state.zoom_factor * 100.0f);
      ImGui::Text("pan: (%.1f, %.1f)", g_state.pan_x, g_state.pan_y);
      ImGui::End();
    }

    ImGui::PopFont();
    ImGui::Render();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}