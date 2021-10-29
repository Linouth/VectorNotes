#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>

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

    GLuint vbo[VBO_count];
    GLuint vao[VAO_count];
    GLuint shader[SHADER_count];

    Vec2 mouse_pos;
    int mouse_state[NUM_MOUSE_STATES];

    unsigned int width, height;
} UI;

UI *ui_init(unsigned width, unsigned height);
void ui_deinit(UI *ui);
void ui_drawSpline(UI *ui, Path *path);
void ui_drawDbgLines(UI *ui, Vec2 *points, size_t count, Rgb color, float linewidth);