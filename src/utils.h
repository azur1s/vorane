#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

#include "imgui.h" // vec2

struct AffineMat3 { float m[9]; };

static AffineMat3 amat3_mul(const AffineMat3 &a, const AffineMat3 &b) {
  AffineMat3 result;
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      result.m[row * 3 + col] =
        a.m[row * 3 + 0] * b.m[0 * 3 + col] +
        a.m[row * 3 + 1] * b.m[1 * 3 + col] +
        a.m[row * 3 + 2] * b.m[2 * 3 + col];
    }
  }
  return result;
}

static AffineMat3 amat3_identity() {
  AffineMat3 result = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
  };
  return result;
}

static AffineMat3 amat3_transform(float tx, float ty) {
  AffineMat3 result = {
    1.0f, 0.0f, tx,
    0.0f, 1.0f, ty,
    0.0f, 0.0f, 1.0f
  };
  return result;
}

static AffineMat3 amat3_scale(float sx, float sy) {
  AffineMat3 result = {
    sx,   0.0f, 0.0f,
    0.0f, sy,   0.0f,
    0.0f, 0.0f, 1.0f
  };
  return result;
}

static AffineMat3 amat3_rotate(float angle_deg) {
  float rad = angle_deg * (3.14159265f / 180.0f);
  float cos_a = std::cos(rad);
  float sin_a = std::sin(rad);
  AffineMat3 result = {
    cos_a, -sin_a, 0.0f,
    sin_a,  cos_a, 0.0f,
    0.0f,   0.0f,  1.0f
  };
  return result;
}

// get memory usage in bytes
inline size_t get_mem_usage() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    return pmc.WorkingSetSize;
  }
  return 0;
#else
  return 0; // TODO implement for other platforms
#endif
}

inline std::string format_bytes(size_t bytes) {
  const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
  size_t s = 0;
  double count = static_cast<double>(bytes);
  while (count >= 1024 && s < 4) {
    s++;
    count /= 1024;
  }
  return std::format("{:.2f} {}", count, suffixes[s]);
}

#define LOG_INFO(fmt, ...)  fprintf(stdout, "[INFO] "  fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  fprintf(stderr, "[WARN] "  fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)