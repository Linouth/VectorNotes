#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "nanovg/nanovg.h"
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/nanovg_gl.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "gl.h"
#include "path.h"
#include "vec.h"
#include "vectornotes.h"

VnCtx g_vn = {0};

static void setViewport(VnCtx *vn, unsigned width, unsigned height) {
    glViewport(0, 0, width, height);

    vn->view_width = width;
    vn->view_height = height;

    for (size_t i = 0; i < sizeof(vn->shaders)/sizeof(GLuint); i++) {
        glProgramUniform2f(
                vn->shaders[i],
                glGetUniformLocation(vn->shaders[i], "viewSize"),
                width, height);
    }
}

static Vec2 canvasToScreen(VnCtx *vn, Vec2 point) {
    return vec2_scalarMult(
            vec2_sub(point, vn->view_origin),
            vn->view_scale);
}

static void canvasToScreenN(VnCtx *vn, Vec2 *dest, Vec2 *src, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dest[i] = canvasToScreen(vn, src[i]);
    }
}

static Vec2 screenToCanvas(VnCtx *vn, Vec2 point) {
    return vec2_add(
            vec2_scalarMult(point, 1/vn->view_scale),
            vn->view_origin);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    VnCtx *vn = &g_vn;

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
            case GLFW_KEY_D:
                vn->debug = !vn->debug;
                break;
            case GLFW_KEY_P: {
                for (size_t i = 0; i < vn->tmp_path->node_cnt; i++) {
                    printf("{%f, %f},\n",
                            vn->tmp_path->nodes[i].x, vn->tmp_path->nodes[i].y);
                }
            } break;

            default:
                break;
        }
    }
}

static void mousePositionCallback(GLFWwindow* window, double xpos, double ypos) {
    VnCtx *vn = &g_vn;

    vn->mouse_pos.x = xpos;
    vn->mouse_pos.y = ypos;

    static Vec2 *prev_node = NULL;
    static double prev_len = 0;

    if (vn->mouse_states[GLFW_MOUSE_BUTTON_LEFT] == GLFW_PRESS) {
        if (!prev_node) {
            prev_node = path_getNode(vn->tmp_path, -1);
        }

        // TODO: This should probably be in some 'pencil' tool module. Maybe
        // have callback functions per tool
        double cmp = 5.0;
        double curr_len = 0;
        Vec2 prev_node_screen_pos = canvasToScreen(vn, *prev_node);
        if (vn->tmp_path->node_cnt > 1) {
            // The last two points, and a tangent vector determined from
            // these points.
            Vec2 *p1 = path_getNode(vn->tmp_path, -1);
            Vec2 *p0 = path_getNode(vn->tmp_path, -2);
            Vec2 tg = vec2_norm(vec2_sub(*p1, *p0));

            // Vector from the last node to the cursor position.
            Vec2 r = vec2_sub(vn->mouse_pos, prev_node_screen_pos);
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
                || vec2_dist(prev_node_screen_pos, vn->mouse_pos) > cmp) {
            Vec2 p = screenToCanvas(vn, vn->mouse_pos);
            path_addNode(vn->tmp_path, p, -1);

            prev_node = path_getNode(vn->tmp_path, -1);
            prev_len = 0;
        }
    } else if (vn->mouse_states[GLFW_MOUSE_BUTTON_RIGHT] == GLFW_PRESS) {
        Vec2 r = vec2_sub(
                screenToCanvas(vn, vn->mouse_pos), vn->mouse_pos_rc);
        vn->view_origin.x += -r.x;
        vn->view_origin.y += -r.y;
    }
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    VnCtx *vn = &g_vn;

    vn->mouse_states[button] = action;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // Button pressed, clear path and start over
            // TODO: Probably better to just set count to 0
            if (vn->tmp_path)
                path_deinit(vn->tmp_path);

            vn->tmp_path = path_init(0);
            vn->tmp_path_ready = false;
        } else {
            // Button released
            if (vn->tmp_path->node_cnt > 1)
                vn->tmp_path_ready = true;
        }

        // Button pressed or released, place point at cursor pos.
        // Only if the prev node is not at the exact same position.
        Vec2 p = screenToCanvas(vn, vn->mouse_pos);
        Vec2 *prev_node = path_getNode(vn->tmp_path, -1);
        if (prev_node->x != p.x || prev_node->y != p.y) {
            path_addNode(vn->tmp_path, p, -1);
            printf("Added a node at %f %f\n", p.x, p.y);
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            vn->mouse_pos_rc = screenToCanvas(vn, vn->mouse_pos);
        }
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    VnCtx *vn = &g_vn;

    // Zoom by changing the scale parameter, and correcting the view_offset to
    // scale around the mouse position.
    if (yoffset != 0.0) {
        const double MAX_ZOOM_IN = 1.5e13;
        const double MAX_ZOOM_OUT = 1.0e-20;
        if (yoffset == 1.0 && vn->view_scale >= MAX_ZOOM_IN)
            return;
        if (yoffset == -1.0 && vn->view_scale <= MAX_ZOOM_OUT)
            return;

        Vec2 mouse_before = screenToCanvas(vn, vn->mouse_pos);

        const double SCALING_FACTOR = 1.05;
        vn->view_scale *= yoffset > 0 ? SCALING_FACTOR : 1/SCALING_FACTOR;

        Vec2 mouse_after = screenToCanvas(vn, vn->mouse_pos);

        Vec2 r = vec2_sub(mouse_after, mouse_before);
        vn->view_origin = vec2_sub(vn->view_origin, r);
    }
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    VnCtx *vn = &g_vn;
    setViewport(vn, width, height);
}

VnCtx *vn_init(unsigned width, unsigned height) {
    //VnCtx *vn = malloc(sizeof(VnCtx));
    VnCtx *vn = &g_vn;
    vn->view_scale = 1.0;

    { // Setup window
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        vn->window = glfwCreateWindow(width, height, "VectorNotes", NULL, NULL);
        if (vn->window == NULL) {
            printf("Failed to create GLFW window\n");
            glfwTerminate();
            return NULL;
        }
        glfwMakeContextCurrent(vn->window);

        // Prepare GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            printf("Failed to initialize GLAD\n");
            return NULL;
        }

        setViewport(vn, width, height);

        glfwSetFramebufferSizeCallback(vn->window, framebufferSizeCallback);
        glfwSetKeyCallback(vn->window, keyCallback);
        glfwSetMouseButtonCallback(vn->window, mouseButtonCallback);
        glfwSetCursorPosCallback(vn->window, mousePositionCallback);
        glfwSetScrollCallback(vn->window, scrollCallback);
    }

    glGenVertexArrays(VAO_count, vn->vaos);
    glGenBuffers(VBO_count, vn->vbos);
    for (int i = 0; i < VAO_count; i++) {
        assert(i < VBO_count);
        glBindVertexArray(vn->vaos[i]);
        glBindBuffer(GL_ARRAY_BUFFER, vn->vbos[i]);

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

        vn->shaders[SHADER_simple] = gl_createProgram(shaders);
    }
    {
        Shader shaders[] = { // SHADER_spline
            { GL_VERTEX_SHADER, true, "glsl/scale.vs" },
            //{ GL_GEOMETRY_SHADER, true, "glsl/stipple.gs" },
            { GL_FRAGMENT_SHADER, true, "glsl/simple.fs" },
            { GL_NONE },
        };

        vn->shaders[SHADER_stipple] = gl_createProgram(shaders);
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
        vn->shaders[SHADER_debug] = gl_createProgram(shaders);
    }

    vn->vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (!vn->vg)
        return NULL;

    return vn;
}

void vn_deinit(VnCtx *vn) {
    for (size_t i = 0; i < sizeof(vn->shaders)/sizeof(GLuint); i++) {
        glDeleteProgram(vn->shaders[i]);
    }
    glDeleteBuffers(sizeof(vn->vbos)/sizeof(GLuint), vn->vbos);
    glDeleteVertexArrays(sizeof(vn->vaos)/sizeof(GLuint), vn->vaos);

    if (vn->tmp_path)
        path_deinit(vn->tmp_path);

    if (vn->vg)
        nvgDeleteGL3(vn->vg);

    glfwDestroyWindow(vn->window);
    glfwTerminate();

    //free(vn);
}

void vn_drawPath(VnCtx *vn, Path *path) {
    assert(vn->vg != NULL);

    nvgBeginPath(vn->vg);
    nvgStrokeColor(vn->vg, nvgRGBA(230, 20, 15, 255));

    Vec2 p = canvasToScreen(vn, path->nodes[0]);
    nvgMoveTo(vn->vg, p.x, p.y);
    for (size_t j = 1; j < path->node_cnt; j+=3) {
        Vec2 p0 = canvasToScreen(vn, path->nodes[j]);
        Vec2 p1 = canvasToScreen(vn, path->nodes[j+1]);
        Vec2 p2 = canvasToScreen(vn, path->nodes[j+2]);

        nvgBezierTo(vn->vg,
                p0.x, p0.y,
                p1.x, p1.y,
                p2.x, p2.y);
    }
    nvgStroke(vn->vg);
}

void vn_drawLines(VnCtx *vn, Path *path) {
    nvgBeginPath(vn->vg);
    nvgStrokeColor(vn->vg, nvgRGBA(82, 144, 242, 255));

    Vec2 p = canvasToScreen(vn, path->nodes[0]);
    nvgMoveTo(vn->vg, p.x, p.y);
    for (size_t i = 1; i < path->node_cnt; i++) {
        p = canvasToScreen(vn, path->nodes[i]);
        nvgLineTo(vn->vg, p.x, p.y);
    }
    nvgStroke(vn->vg);
}

void vn_drawCtrlPoints(VnCtx *vn, Path *path) {
    GLuint color_loc;

    Vec2 *nodes = malloc(sizeof(Vec2) * path->node_cnt);
    canvasToScreenN(vn, nodes, path->nodes, path->node_cnt);

    glBindVertexArray(vn->vaos[VAO_spline]);

    glBindBuffer(GL_ARRAY_BUFFER, vn->vbos[VBO_spline]);
    glBufferData(GL_ARRAY_BUFFER, path->node_cnt*sizeof(Vec2), nodes, GL_DYNAMIC_DRAW);

    {
        glUseProgram(vn->shaders[SHADER_simple]);
        glPointSize(2.0f);
        color_loc = glGetUniformLocation(vn->shaders[SHADER_simple], "color");

        glUniform4f(color_loc, 0.70, 0.70, 0.70, 1.0);
        glDrawArrays(GL_POINTS, 0, path->node_cnt);
    }


    {
        glUseProgram(vn->shaders[SHADER_stipple]);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(1.0f);
        color_loc = glGetUniformLocation(vn->shaders[SHADER_stipple], "color");

        glUniform4f(color_loc, 0.173, 0.325, 0.749, 0.1);
        glDrawArrays(GL_LINE_STRIP, 0, path->node_cnt);
    }

    free(nodes);
}

void vn_drawDbgLines(VnCtx *vn, Vec2 *points, size_t count, Rgb color, float linewidth) {
    Vec2 *p = malloc(sizeof(Vec2) * count);
    canvasToScreenN(vn, p, points, count);

    glUseProgram(vn->shaders[SHADER_debug]);
    glBindVertexArray(vn->vaos[VAO_debug]);

    glBindBuffer(GL_ARRAY_BUFFER, vn->vbos[VBO_debug]);
    glBufferData(GL_ARRAY_BUFFER, count*sizeof(Vec2), p, GL_DYNAMIC_DRAW);

    GLuint color_loc = glGetUniformLocation(vn->shaders[SHADER_debug], "color");
    glUniform4f(color_loc, color.r, color.g, color.b, 1.0);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(linewidth);
    glDrawArrays(GL_LINES, 0, count);

    free(p);
}
