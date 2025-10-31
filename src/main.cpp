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
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imnodes.h"

#include <stdio.h>
#include <format>
#include <memory>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "nodes.hpp"
#include "shader.hpp"
#include "style.hpp"
#include "utils.hpp"

#define INIT_FLOAT4(v, a, b, c, d) \
  v[0] = a; \
  v[1] = b; \
  v[2] = c; \
  v[3] = d;

struct State {
  int canvas_w = 0; // init later
  int canvas_h = 0;
  FBO ping, pong;
  int next_op_id = 0;
  std::vector<std::unique_ptr<Op>> ops;
  int output_node_id = -1;

  float zoom_factor   = 1.0f;
  float last_mouse_x  = 0.0f;
  float last_mouse_y  = 0.0f;
  float pan_x         = 0.0f;
  float pan_y         = 0.0f;
  bool isfullscreen   = false;
  bool isopen_editor  = true;
  bool isconfirm_exit = false;

  void init() {
    ops.reserve(16);

    int init_w = 512, init_h = 512;
    set_canvas_size(init_w, init_h);

    // init vao & fbos
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    ping.tex.createRGBA8(canvas_w, canvas_h);
    ping.create(canvas_w, canvas_h);
    pong.tex.createRGBA8(canvas_w, canvas_h);
    pong.create(canvas_w, canvas_h);
  }

  void register_op(std::unique_ptr<Op> op) {
    op->id = next_op_id++;
    if (op->get_type_name() == std::string("const/output")) {
      output_node_id = op->id;
    }
    ops.push_back(std::move(op));
  }

  std::unique_ptr<Op>& get_op_by_id(int id) {
    for (auto& op : ops) {
      if (op->id == id) {
        return op;
      }
    }
    static std::unique_ptr<Op> null_op = nullptr;
    return null_op;
  }

  void unregister_op(int id) {
    ops.erase(
      std::remove_if(
        ops.begin(),
        ops.end(),
        [id](const std::unique_ptr<Op>& op) { return op->id == id; }
      ),
      ops.end()
    );
  }

  void render(Op* op, const std::vector<GLuint>& input_textures) {
    op->apply(
      input_textures,
      canvas_w,
      canvas_h
    );
  }

  GLuint eval(int root_id) {
    std::unique_ptr<Op>& root_op = get_op_by_id(root_id);
    if (!root_op) {
      return 0;
    } else {
      std::vector<GLuint> input_textures;
      input_textures.reserve(root_op->input_ids.size());

      for (int input_id : root_op->input_ids) {
        if (input_id == root_op->id) {
          LOG_WARN("Detected self-referencing input for op id=%d, skipping", root_op->id);
          continue;
        }
        input_textures.push_back(eval(input_id));
      }

      render(root_op.get(), input_textures);
      return root_op->layer_fbo.tex.id;
    }
  }

  void set_canvas_size(int w, int h) {
    canvas_w = w;
    canvas_h = h;
    ping.resize(canvas_w, canvas_h);
    pong.resize(canvas_w, canvas_h);
  }
};
static State g_state;

void window_close_callback(GLFWwindow *window) {
  if (!g_state.isconfirm_exit) {
    g_state.isconfirm_exit = true;
  }
  glfwSetWindowShouldClose(window, GLFW_FALSE);
  return;
}

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

  g_state.init();

  Texture input_texture;
  input_texture.createRGBA8(g_state.canvas_w, g_state.canvas_h);

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

    if (ImGui::IsKeyPressed(ImGuiKey_E) && !io.WantCaptureKeyboard) {
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

    GLuint final_tex = input_texture.id;
    if (g_state.output_node_id >= 0) {
      final_tex = g_state.eval(g_state.output_node_id);
    }

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
            g_state.register_op(std::make_unique<decltype(op)>(op)); \
          }

        if (ImGui::Selectable("const/color")) {
          OpConstColor op; CHECK_PROG_ID_AND_PUSH(op);
        }
        if (ImGui::Selectable("const/image")) {
          OpConstImage op(""); CHECK_PROG_ID_AND_PUSH(op);
        }
        if (ImGui::Selectable("gen/composite")) {
          OpGenComposite op; CHECK_PROG_ID_AND_PUSH(op);
        }
        if (ImGui::Selectable("gen/transform")) {
          OpGenTransform op; CHECK_PROG_ID_AND_PUSH(op);
        }
        ImGui::EndCombo();
      }

      ImGui::Separator();

      ImGui::Text("operations:");

      for (size_t i = 0; i < g_state.ops.size(); i++) {
        Op &operation = *(g_state.ops[i]);
        const char *type_name = operation.get_type_name();

        const char *header = std::format(
          "{} - {}{}", operation.id, type_name, operation.bypass ? " (bypassed)" : ""
        ).c_str();

        ImGui::SetNextItemOpen(true, ImGuiCond_Once);

        if (ImGui::CollapsingHeader(header)) {
          ImGui::Indent();
          if (ImGui::Button(format_id("set as output", i))) {
            g_state.output_node_id = g_state.ops[i]->id;
            LOG_INFO("Set operation %d as output node", g_state.ops[i]->id);
          }
          if (ImGui::Button(format_id("bypass", i))) {
            g_state.ops[i]->bypass = !g_state.ops[i]->bypass;
          }
          ImGui::SameLine();
          if (ImGui::Button(format_id("remove", i))) {
            if (g_state.output_node_id == g_state.ops[i]->id) {
              g_state.output_node_id = -1;
            }

            g_state.unregister_op(g_state.ops[i]->id);
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

          // inputs
          if (ImGui::Button(format_id("add input", i))) {
            g_state.ops[i]->input_ids.push_back(-1);
          }

          for (size_t j = 0; j < operation.input_ids.size(); j++) {
            ImGui::PushID(j);
            ImGui::Text("input %d:", (int)j);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::InputInt(
              format_id("##input_id", i),
              &operation.input_ids[j]
            );
            ImGui::SameLine();
            if (ImGui::Button(format_id("remove input", i))) {
              operation.input_ids.erase(operation.input_ids.begin() + j);
              ImGui::PopID();
              j--;
              continue;
            }
            ImGui::PopID();
          }

          operation.ui(i);

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