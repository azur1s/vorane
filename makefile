CC := g++

BUILD_DIR := build
TARGET:= $(BUILD_DIR)/vorane

SRCS := $(wildcard src/*.cc)
EXTERNAL_CPP_SRCS := \
	external/imgui/imgui.cpp \
	external/imgui/imgui_draw.cpp \
	external/imgui/imgui_tables.cpp \
	external/imgui/imgui_widgets.cpp \
	external/imgui/backends/imgui_impl_glfw.cpp \
	external/imgui/backends/imgui_impl_opengl3.cpp
EXTERNAL_C_SRCS := \
	external/glad/src/glad.c
ALL_SRCS := $(SRCS) $(EXTERNAL_CPP_SRCS) $(EXTERNAL_C_SRCS)

OBJ_SRCS := $(SRCS:.cc=.o) \
	$(EXTERNAL_CPP_SRCS:.cpp=.o) \
	$(EXTERNAL_C_SRCS:.c=.o)
OBJS := $(patsubst %,$(BUILD_DIR)/%,$(OBJ_SRCS))

CFLAGS := -Wall -Wextra -Wpedantic \
	-std=c++20 \
	-I./external/imgui -I./external/imgui/backends \
	-I./external/glad/include
# extra flags
# -DASK_FOR_HIGH_PERFORMANCE_GPU
#   request high performance GPU on laptops with dual GPU
CFLAGS += \
# 	-DASK_FOR_HIGH_PERFORMANCE_GPU
LDFLAGS := -lglfw3 -lopengl32

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
