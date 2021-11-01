#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "nanovg/nanovg.h"
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/nanovg_gl.h"


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "gl.h"
#include "path.h"
#include "ui.h"
#include "vec.h"

UI g_ui = {0};

static void setViewport(UI *ui, unsigned width, unsigned height) {
    glViewport(0, 0, width, height);

    ui->view_width = width;
    ui->view_height = height;

    for (size_t i = 0; i < sizeof(ui->shaders)/sizeof(GLuint); i++) {
        glProgramUniform2f(
                ui->shaders[i],
                glGetUniformLocation(ui->shaders[i], "viewSize"),
                width, height);
    }
}

static Vec2 canvasToScreen(UI *ui, Vec2 point) {
    return vec2_scalarMult(
            vec2_sub(point, ui->view_origin),
            ui->view_scale);
}

static void canvasToScreenN(UI *ui, Vec2 *dest, Vec2 *src, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dest[i] = canvasToScreen(ui, src[i]);
    }
}

static Vec2 screenToCanvas(UI *ui, Vec2 point) {
    return vec2_add(
            vec2_scalarMult(point, 1/ui->view_scale),
            ui->view_origin);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_ESCAPE:
            case GLFW_KEY_Q:
                glfwSetWindowShouldClose(window, true);
                break;
            case GLFW_KEY_M: {
                static int mode = 0;
                printf("%d\n", mode);
                glPolygonMode(GL_FRONT_AND_BACK, GL_POINT + mode);
                mode = (mode + 1) % 3;
            } break;
            default:
                break;
        }
    }
}

static void mousePositionCallback(GLFWwindow* window, double xpos, double ypos) {
    UI *ui = &g_ui;

    ui->mouse_pos.x = xpos;
    ui->mouse_pos.y = ypos;

    static Vec2 *prev_node = NULL;
    static double prev_len = 0;

    if (ui->mouse_states[GLFW_MOUSE_BUTTON_LEFT] == GLFW_PRESS) {
        if (!prev_node) {
            prev_node = path_getNode(ui->tmp_path, -1);
        }

        // TODO: This should probably be in some 'pencil' tool module. Maybe
        // have callback functions per tool
        double cmp = 5.0;
        double curr_len = 0;
        Vec2 prev_node_screen_pos = canvasToScreen(ui, *prev_node);
        if (ui->tmp_path->node_cnt > 1) {
            // The last two points, and a tangent vector determined from
            // these points.
            Vec2 *p1 = path_getNode(ui->tmp_path, -1);
            Vec2 *p0 = path_getNode(ui->tmp_path, -2);
            Vec2 tg = vec2_norm(vec2_sub(*p1, *p0));

            // Vector from the last node to the cursor position.
            Vec2 r = vec2_sub(ui->mouse_pos, prev_node_screen_pos);
            curr_len = vec2_len(r);

            // Indicator of angle between tangent vector and cursor vector.
            // NOTE: tg is unit length
            double alpha = vec2_dot(r, tg) / curr_len;

            // Determine the length required for a new node to be placed.
            // Line segments can be at most 'max_len' long, and min 'min_len' long.
            // The exponent determines how aggressive the node-placing is.
            const double max_len = 128.0;
            const double min_len = 7.0;
            const double exponent = 512.0;
            cmp = max_len*pow(alpha, exponent) + min_len;

            // Whenever the cursor moves back, place a node to capture this movement
            if (curr_len > prev_len)
                prev_len = curr_len;
        }

        if (curr_len < (prev_len - 5.0)
                || vec2_dist(prev_node_screen_pos, ui->mouse_pos) > cmp) {
            Vec2 p = screenToCanvas(ui, ui->mouse_pos);
            path_addNode(ui->tmp_path, p, -1);

            prev_node = path_getNode(ui->tmp_path, -1);
            prev_len = 0;
        }
    } else if (ui->mouse_states[GLFW_MOUSE_BUTTON_RIGHT] == GLFW_PRESS) {
        Vec2 r = vec2_sub(
                screenToCanvas(ui, ui->mouse_pos), ui->mouse_pos_rc);
        ui->view_origin.x += -r.x;
        ui->view_origin.y += -r.y;
    }
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    UI *ui = &g_ui;

    ui->mouse_states[button] = action;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // Button pressed, clear path and start over
            // TODO: Probably better to just set count to 0
            if (ui->tmp_path)
                path_deinit(ui->tmp_path);

            ui->tmp_path = path_init(0);
            ui->tmp_path_ready = false;
        } else {
            // Button released
            ui->tmp_path_ready = true;
        }

        // Button pressed or released, place point at cursor pos
        Vec2 p = screenToCanvas(ui, ui->mouse_pos);
        path_addNode(ui->tmp_path, p, -1);
        printf("Added a node at %f %f\n",
                p.x, p.y);
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            ui->mouse_pos_rc = screenToCanvas(ui, ui->mouse_pos);
        }
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    UI *ui = &g_ui;

    // Zoom by changing the scale parameter, and correcting the view_offset to
    // scale around the mouse position.
    if (yoffset != 0.0) {
        Vec2 mouse_before = screenToCanvas(ui, ui->mouse_pos);

        const double SCALING_FACTOR = 1.05;
        ui->view_scale *= yoffset > 0 ? SCALING_FACTOR : 1/SCALING_FACTOR;

        Vec2 mouse_after = screenToCanvas(ui, ui->mouse_pos);

        Vec2 r = vec2_sub(mouse_after, mouse_before);
        ui->view_origin = vec2_sub(ui->view_origin, r);
    }
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    UI *ui = &g_ui;
    setViewport(ui, width, height);
}

UI *ui_init(unsigned width, unsigned height) {
    //UI *ui = malloc(sizeof(UI));
    UI *ui = &g_ui;
    ui->view_scale = 1.0;

    { // Setup window
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        ui->window = glfwCreateWindow(width, height, "VectorNotes", NULL, NULL);
        if (ui->window == NULL) {
            printf("Failed to create GLFW window\n");
            glfwTerminate();
            return NULL;
        }
        glfwMakeContextCurrent(ui->window);

        // Prepare GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            printf("Failed to initialize GLAD\n");
            return NULL;
        }

        setViewport(ui, width, height);

        glfwSetFramebufferSizeCallback(ui->window, framebufferSizeCallback);
        glfwSetKeyCallback(ui->window, keyCallback);
        glfwSetMouseButtonCallback(ui->window, mouseButtonCallback);
        glfwSetCursorPosCallback(ui->window, mousePositionCallback);
        glfwSetScrollCallback(ui->window, scrollCallback);
    }

    glGenVertexArrays(VAO_count, ui->vaos);
    glGenBuffers(VBO_count, ui->vbos);
    for (int i = 0; i < VAO_count; i++) {
        assert(i < VBO_count);
        glBindVertexArray(ui->vaos[i]);
        glBindBuffer(GL_ARRAY_BUFFER, ui->vbos[i]);

        switch (i) {
        case VAO_spline:
            glVertexAttribPointer(0, 2, GL_DOUBLE, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(0);
            break;
        case VAO_debug:
            glVertexAttribPointer(0, 2, GL_DOUBLE, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(0);
            break;

        default: break;
        }
    }
    glBindVertexArray(0);

    // TODO: Check if createProgram was successful. Also, this could be done
    // programmatically
    {
        Shader shaders[] = { // SHADER_spline
            { GL_VERTEX_SHADER, true, "glsl/scale.vs" },
            { GL_FRAGMENT_SHADER, true, "glsl/simple.fs" },
            { GL_NONE },
        };

        ui->shaders[SHADER_simple] = gl_createProgram(shaders);
    }
    {
        Shader shaders[] = { // SHADER_spline
            { GL_VERTEX_SHADER, true, "glsl/scale.vs" },
            { GL_GEOMETRY_SHADER, true, "glsl/stipple.gs" },
            { GL_FRAGMENT_SHADER, true, "glsl/simple.fs" },
            { GL_NONE },
        };

        ui->shaders[SHADER_stipple] = gl_createProgram(shaders);
    }
    {
        Shader shaders[] = { // SHADER_debug
            { GL_VERTEX_SHADER, true, "glsl/scale.vs" },
            //{ GL_TESS_CONTROL_SHADER, true, "glsl/bezier.tcs" },
            //{ GL_TESS_EVALUATION_SHADER, true, "glsl/bezier.tes" },
            //{ GL_GEOMETRY_SHADER, true, "glsl/stroke.gs" },
            { GL_FRAGMENT_SHADER, true, "glsl/simple.fs" },
            { GL_NONE },
        };
        ui->shaders[SHADER_debug] = gl_createProgram(shaders);
    }

    ui->vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (!ui->vg)
        return NULL;

    return ui;
}

void ui_deinit(UI *ui) {
    for (size_t i = 0; i < sizeof(ui->shaders)/sizeof(GLuint); i++) {
        glDeleteProgram(ui->shaders[i]);
    }
    glDeleteBuffers(sizeof(ui->vbos)/sizeof(GLuint), ui->vbos);
    glDeleteVertexArrays(sizeof(ui->vaos)/sizeof(GLuint), ui->vaos);

    glfwDestroyWindow(ui->window);

    if (ui->tmp_path)
        path_deinit(ui->tmp_path);

    if (ui->vg)
        nvgDeleteGL3(ui->vg);

    //free(ui);
}

void ui_drawPath(UI *ui, Path *path) {
    assert(ui->vg != NULL);

    nvgBeginPath(ui->vg);
    nvgStrokeColor(ui->vg, nvgRGBA(230, 20, 15, 255));

    Vec2 p = canvasToScreen(ui, path->nodes[0]);
    nvgMoveTo(ui->vg, p.x, p.y);
    for (size_t j = 1; j < path->node_cnt; j+=3) {
        Vec2 p0 = canvasToScreen(ui, path->nodes[j]);
        Vec2 p1 = canvasToScreen(ui, path->nodes[j+1]);
        Vec2 p2 = canvasToScreen(ui, path->nodes[j+2]);

        nvgBezierTo(ui->vg,
                p0.x, p0.y,
                p1.x, p1.y,
                p2.x, p2.y);
    }
    nvgStroke(ui->vg);
}

void ui_drawLines(UI *ui, Path *path) {
    nvgBeginPath(ui->vg);
    nvgStrokeColor(ui->vg, nvgRGBA(82, 144, 242, 255));

    Vec2 p = canvasToScreen(ui, path->nodes[0]);
    nvgMoveTo(ui->vg, p.x, p.y);
    for (size_t i = 1; i < path->node_cnt; i++) {
        p = canvasToScreen(ui, path->nodes[i]);
        nvgLineTo(ui->vg, p.x, p.y);
    }
    nvgStroke(ui->vg);
}

void ui_drawCtrlPoints(UI *ui, Path *path) {
    GLuint color_loc;

    Vec2 *nodes = malloc(sizeof(Vec2) * path->node_cnt);
    canvasToScreenN(ui, nodes, path->nodes, path->node_cnt);

    glUseProgram(ui->shaders[SHADER_simple]);
    glBindVertexArray(ui->vaos[VAO_spline]);

    glBindBuffer(GL_ARRAY_BUFFER, ui->vbos[VBO_spline]);
    glBufferData(GL_ARRAY_BUFFER, path->node_cnt*sizeof(Vec2), nodes, GL_DYNAMIC_DRAW);

    glPointSize(4.0f);
    color_loc = glGetUniformLocation(ui->shaders[SHADER_simple], "color");

    glUniform3f(color_loc, 0.70, 0.70, 0.70);
    glDrawArrays(GL_POINTS, 0, path->node_cnt);


    glUseProgram(ui->shaders[SHADER_stipple]);

    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1.0f);
    color_loc = glGetUniformLocation(ui->shaders[SHADER_stipple], "color");

    glUniform3f(color_loc, 0.173, 0.325, 0.749);
    glDrawArrays(GL_LINE_STRIP, 0, path->node_cnt);

    free(nodes);
}

void ui_drawDbgLines(UI *ui, Vec2 *points, size_t count, Rgb color, float linewidth) {
    Vec2 *p = malloc(sizeof(Vec2) * count);
    canvasToScreenN(ui, p, points, count);

    glUseProgram(ui->shaders[SHADER_debug]);
    glBindVertexArray(ui->vaos[VAO_debug]);

    glBindBuffer(GL_ARRAY_BUFFER, ui->vbos[VBO_debug]);
    glBufferData(GL_ARRAY_BUFFER, count*sizeof(Vec2), p, GL_DYNAMIC_DRAW);

    GLuint color_loc = glGetUniformLocation(ui->shaders[SHADER_debug], "color");
    glUniform3f(color_loc, color.r, color.g, color.b);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(linewidth);
    glDrawArrays(GL_LINES, 0, count);

    free(p);
}
