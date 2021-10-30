#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "nanovg/nanovg.h"

#include <stdlib.h>
#include <stdbool.h>

#include "vec.h"
#include "path.h"

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
typedef struct ui_ctx {
    GLFWwindow *window;

    GLuint vbos[VBO_count];
    GLuint vaos[VAO_count];
    GLuint shaders[SHADER_count];

    NVGcontext *vg;

    Vec2 mouse_pos;
    int mouse_states[NUM_MOUSE_STATES];
    Vec2 mouse_pos_rc;

    unsigned view_width, view_height;
    Vec2 view_origin;
    double view_scale;

    // TODO: This should be part of some 'pencil' tool
    Path *tmp_path;
    bool tmp_path_ready;
} UI;

UI *ui_init(unsigned width, unsigned height);
void ui_deinit(UI *ui);
void ui_drawPath(UI *ui, Path *path);
void ui_drawLines(UI *ui, Path *path);
void ui_drawCtrlPoints(UI *ui, Path *path);
void ui_drawDbgLines(UI *ui, Vec2 *points, size_t count, Rgb color, float linewidth);
