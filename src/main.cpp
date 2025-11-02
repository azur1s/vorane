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
#include "fonts/geist_mono.h"

#define INIT_FLOAT4(v, a, b, c, d) \
  v[0] = a; \
  v[1] = b; \
  v[2] = c; \
  v[3] = d;

struct Link {
  int id;
  int start_attr; // output attribute id
  int end_attr;   // input attribute id
};

struct State {
  int canvas_w = 0; // init later
  int canvas_h = 0;
  FBO ping, pong;
  int next_op_id = 0;
  std::vector<std::unique_ptr<Op>> ops;
  int output_node_id = -1;

  std::vector<Link> links;
  int next_link_id = 0;

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

  void create_link(int start_attr, int end_attr) {
    Link link;
    link.id = next_link_id++;
    link.start_attr = start_attr;
    link.end_attr = end_attr;
    links.push_back(link);
  }

  void remove_link(int link_id) {
    links.erase(
      std::remove_if(
        links.begin(),
        links.end(),
        [link_id](const Link& link) { return link.id == link_id; }
      ),
      links.end()
    );
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
    if (output_node_id == id) {
      output_node_id = -1;
    }

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
  // io.IniFilename = NULL;
  ImGui::LoadIniSettingsFromDisk("./imgui.ini");
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
  io.Fonts->AddFontFromMemoryCompressedTTF(
    geist_mono_compressed_data,
    geist_mono_compressed_size,
    14.0f
  );

  ImNodes::CreateContext();
  ImNodes::StyleColorsDark();
  ImNodesIO& imnodes_io = ImNodes::GetIO();
  imnodes_io.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;

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
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

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
      g_state.pan_x += dx;
      g_state.pan_y += dy;
    }
    g_state.last_mouse_x = io.MousePos.x;
    g_state.last_mouse_y = io.MousePos.y;

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q)) {
      g_state.isconfirm_exit = true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
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

    float offx = g_state.pan_x / (float)display_w * 2.0f;
    float offy = -g_state.pan_y / (float)display_h * 2.0f;
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
        if (ImGui::MenuItem("exit", "C-Q")) {
          g_state.isconfirm_exit = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("view")) {
        if (ImGui::MenuItem("editor", "Tab", g_state.isopen_editor)) {
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
    if (g_state.isopen_editor) {
      ImGui::Begin("editor", &g_state.isopen_editor, ImGuiWindowFlags_NoCollapse);
      ImNodes::BeginNodeEditor();

      // context menu
      {
        const bool open_popup = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
          && ImNodes::IsEditorHovered()
          && ImGui::IsMouseClicked(ImGuiMouseButton_Right);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));

        if (!ImGui::IsAnyItemHovered() && open_popup) {
          ImGui::OpenPopup("add op");
        }

        if (ImGui::BeginPopup("add op")) {
          const ImVec2 click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

          #define CHECK_PROG_ID_AND_PUSH(op) \
            if (!op.prog_id) { \
              LOG_ERROR("Failed to create shader for %s", op.get_type_name()); \
            } else { \
              g_state.register_op(std::make_unique<decltype(op)>(op)); \
              ImNodes::SetNodeScreenSpacePos( \
                g_state.next_op_id - 1, \
                click_pos \
              ); \
            }

          bool over_limit = g_state.ops.size() >= 65535;
          if (ImGui::MenuItem("const/color") && !over_limit) {
            OpConstColor op; CHECK_PROG_ID_AND_PUSH(op);
          }
          if (ImGui::MenuItem("const/image") && !over_limit) {
            OpConstImage op(""); CHECK_PROG_ID_AND_PUSH(op);
          }
          if (ImGui::MenuItem("gen/composite") && !over_limit) {
            OpGenComposite op; CHECK_PROG_ID_AND_PUSH(op);
          }
          if (ImGui::MenuItem("gen/transform") && !over_limit) {
            OpGenTransform op; CHECK_PROG_ID_AND_PUSH(op);
          }
          if (ImGui::MenuItem("gen/grade") && !over_limit) {
            OpGenGrade op; CHECK_PROG_ID_AND_PUSH(op);
          }
          if (ImGui::MenuItem("eff/blur") && !over_limit) {
            OpEffBlur op; CHECK_PROG_ID_AND_PUSH(op);
          }
          ImGui::EndPopup();
        }

        ImGui::PopStyleVar();
      }

      for (size_t i = 0; i < g_state.ops.size(); i++) {
        Op &op = *(g_state.ops[i]);
        size_t input_count = op.input_names.size();

        // node rendering
        // we can deterministically generate attribute/link ids based on op.id
        // by convention:
        //   [0000 0000 0000 0000 0000 0000 0000 0000] (rightmost are LSB)
        //    unused  | |       | |-----------------| unique per attribute (op.id)
        //            | |-------| input/output attribute ID (max of 255 per op)
        //            | input/output
        // this means that we can have up to 65535 ops before collisions occur
        // we do this because we don't want to clutter Op's struct with extra IDs
        // for input/output attribute IDs
        // let's see if this will scale well enough...

        #define U 16 // bits for op id
        // decode node ID by masking out lower U bits and &ing with the ID
        #define DECODE_NODE_ID(op_id) (op_id & ((1u << U) - 1u))
        // decode attribute ID by shifting right U bits and &ing with 0xFF (8 bits)
        #define DECODE_ATTR_ID(op_id) ((op_id >> U) & 0xFFu)
        #define DECODE_IO(op_id) ((op_id >> (U + 8)) & 0x1u) // 0=input, 1=output

        #define ENCODE_ATTR_ID(op_id, attr_id, is_output) \
          (op_id | ((attr_id & 0xFFu) << U) | ((is_output & 0x1u) << (U + 8)))

        {
          ImNodes::BeginNode(op.id);

          ImNodes::BeginNodeTitleBar();
          ImGui::TextUnformatted(std::format(
            "{}{}", op.get_type_name(), op.bypass ? " (bypassed)" : ""
          ).c_str());
          ImNodes::EndNodeTitleBar();

          for (size_t j = 0; j < input_count; j++) {
            ImNodes::BeginInputAttribute(ENCODE_ATTR_ID(op.id, (int)j, 0));
            const char *label = op.input_names[j];
            ImGui::TextUnformatted(label);

            ImNodes::EndInputAttribute();
          }

          ImNodes::BeginOutputAttribute(ENCODE_ATTR_ID(op.id, 0, 1));
          ImGui::TextUnformatted("output");
          ImNodes::EndOutputAttribute();

          separator(100.0f, 10.0f);
          ImGui::PushItemWidth(200.0f);
          op.ui(i);
          ImGui::PopItemWidth();
          separator(100.0f, 10.0f);

          if (g_state.output_node_id == g_state.ops[i]->id) {
            ImGui::BeginDisabled();
            ImGui::Button("set as output");
            ImGui::EndDisabled();
          } else {
            if (ImGui::Button(format_id("set as output", i))) {
              g_state.output_node_id = g_state.ops[i]->id;
              LOG_INFO("Set operation %d as output node", g_state.ops[i]->id);
            }
          }

          ImNodes::EndNode();
        }
      } // for each op

      for (const Link& link : g_state.links) {
        ImNodes::Link(link.id, link.start_attr, link.end_attr);
      }

      ImNodes::EndNodeEditor();

      // new links
      {
        int start_attr, end_attr;
        if (ImNodes::IsLinkCreated(&start_attr, &end_attr)) {
          int start_op_id = DECODE_NODE_ID(start_attr);
          int end_op_id = DECODE_NODE_ID(end_attr);
          int end_input_idx = DECODE_ATTR_ID(end_attr);

          std::unique_ptr<Op>& end_op = g_state.get_op_by_id(end_op_id);
          if (end_op) {
            // If there's an existing link targeting the same input, remove it first.
            auto it = std::find_if(
              g_state.links.begin(),
              g_state.links.end(),
              [end_attr](const Link& l) { return l.end_attr == end_attr; }
            );
            if (it != g_state.links.end()) {
              int old_link_id = it->id;
              LOG_INFO("Replacing existing link id=%d for input attr=%d", old_link_id, end_attr);

              // Clear the corresponding input_id in the end op of the old link
              int old_end_attr = it->end_attr;
              int old_end_op_id = DECODE_NODE_ID(old_end_attr);
              int old_end_input_idx = DECODE_ATTR_ID(old_end_attr);
              std::unique_ptr<Op>& old_end_op = g_state.get_op_by_id(old_end_op_id);
              if (old_end_op) {
                if (static_cast<size_t>(old_end_input_idx) < old_end_op->input_names.size()) {
              old_end_op->input_ids[old_end_input_idx] = -1;
                }
              }

              g_state.remove_link(old_link_id);
            }

            // Attach the new link
            if (static_cast<size_t>(end_input_idx) < end_op->input_names.size()) {
              end_op->input_ids[end_input_idx] = start_op_id;
              g_state.create_link(start_attr, end_attr);
              LOG_INFO("Created link from op %d to input index %d of op %d",
                  start_op_id, end_input_idx, end_op_id);
            } else {
              LOG_WARN("Input index %d out of range for op id=%d",
                  end_input_idx, end_op_id);
            }
          }
        }
      }

      // delete links
      {
        const int num_selected = ImNodes::NumSelectedLinks();
        if (num_selected > 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
          static std::vector<int> selected_links;
          selected_links.resize(num_selected);
          ImNodes::GetSelectedLinks(selected_links.data());
          for (const int link_id : selected_links) {
            LOG_INFO("Deleting link id=%d", link_id);
            // find which op's input_ids to clear
            auto it = std::find_if(
              g_state.links.begin(),
              g_state.links.end(),
              [link_id](const Link& link) { return link.id == link_id; }
            );
            if (it != g_state.links.end()) {
              // clear the corresponding input_id in the end op
              int end_attr = it->end_attr;
              int end_op_id = DECODE_NODE_ID(end_attr);
              int end_input_idx = DECODE_ATTR_ID(end_attr);

              std::unique_ptr<Op>& end_op = g_state.get_op_by_id(end_op_id);
              if (end_op) {
                LOG_INFO("Clearing input index %d of op id=%d",
                        end_input_idx, end_op_id);
                if (static_cast<size_t>(end_input_idx) < end_op->input_names.size()) {
                  end_op->input_ids[end_input_idx] = -1;
                }
              }
            } else {
              LOG_WARN("Link id=%d not found in state", link_id);
            }

            g_state.remove_link(link_id);
          }
        }
      }

      // delete nodes
      {
        const int num_selected = ImNodes::NumSelectedNodes();
        if (num_selected > 0 && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
          static std::vector<int> selected_nodes;
          selected_nodes.resize(num_selected);
          ImNodes::GetSelectedNodes(selected_nodes.data());
          for (const int node_id : selected_nodes) {
            LOG_INFO("Deleting node id=%d", node_id);
            // remove associated links
            g_state.links.erase(
              std::remove_if(
                g_state.links.begin(),
                g_state.links.end(),
                [node_id](const Link& link) {
                  int start_op_id = DECODE_NODE_ID(link.start_attr);
                  int end_op_id = DECODE_NODE_ID(link.end_attr);
                  return start_op_id == node_id || end_op_id == node_id;
                }
              ),
              g_state.links.end()
            );
            // remove the node
            g_state.unregister_op(node_id);
            // clear output node if needed
            if (g_state.output_node_id == node_id) {
              g_state.output_node_id = -1;
            }
          }
        }
      }

      ImGui::End();

      ImGui::Begin("profiler", nullptr, ImGuiWindowFlags_NoCollapse);

      ImGui::InputInt("canvas width", &g_state.canvas_w);
      ImGui::InputInt("canvas height", &g_state.canvas_h);  

      ImGui::Separator();

      ImGui::Text("canvas: %d x %d", g_state.canvas_w, g_state.canvas_h);
      ImGui::Text("mem: %s", format_bytes(get_mem_usage()).c_str());
      ImGui::Text("fps: %.1f", io.Framerate);
      ImGui::Text("zoom: %.2f%%", g_state.zoom_factor * 100.0f);
      ImGui::Text("pan: (%.1f, %.1f)", g_state.pan_x, g_state.pan_y);
      ImGui::End();
    } // if editor open 

    ImGui::PopFont();
    ImGui::Render();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImNodes::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}