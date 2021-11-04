#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "nanovg/nanovg.h"

#include <stdbool.h>
#include <stdlib.h>

#include "path.h"
#include "tool.h"
#include "vec.h"

typedef struct rgb {
    float r;
    float g;
    float b;
} Rgb;

enum vbo_type {
    VBO_spline,
    VBO_debug,
    VBO_count,
};

enum vao_type {
    VAO_spline,
    VAO_debug,
    VAO_count,
};

enum shader_type {
    SHADER_simple,
    SHADER_stipple,
    SHADER_debug,
    SHADER_count,
};

#define NUM_MOUSE_STATES 8
#define DEFAULT_PATH_CAPACITY 8
typedef struct vn_ctx {
    GLFWwindow *window;

    GLuint vbos[VBO_count];
    GLuint vaos[VAO_count];
    GLuint shaders[SHADER_count];

    NVGcontext *vg;

    unsigned view_width, view_height;
    Vec2 view_origin;
    double view_scale;

    Vec2 mouse_pos;
    Vec2 mouse_pos_rc;  // Mouse pos on right-click
    int mouse_states[NUM_MOUSE_STATES];

    Path **paths;
    size_t path_cnt;
    size_t path_capacity;

    Tool *tools[TOOLS_count];
    size_t tool_cnt;
    size_t active_tool;

    bool debug;
} VnCtx;

VnCtx *vn_init(unsigned width, unsigned height);
void vn_deinit(VnCtx *vn);
void vn_update(VnCtx *vn);
void vn_drawPath(VnCtx *vn, Path *path);
void vn_drawLines(VnCtx *vn, Path *path);
void vn_drawCtrlPoints(VnCtx *vn, Path *path);
void vn_drawDbgLines(VnCtx *vn, Vec2 *points, size_t count, Rgb color, float linewidth);

// TODO: Think of a better solution than using a global instance of vn.
// Possibly having the 'canvas' as a separate ui module
Vec2 canvasToScreen(Vec2 point);
void canvasToScreenN(Vec2 *dest, Vec2 *src, size_t count);
Vec2 screenToCanvas(Vec2 point);
