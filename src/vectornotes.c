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
#include "tool.h"
#include "vec.h"
#include "vectornotes.h"

VnCtx g_vn = {0};

// TODO: Think of a better solution than using a global instance of vn.
// Possibly having the 'canvas' as a separate ui module
Vec2 canvasToScreen(Vec2 point) {
    VnCtx *vn = &g_vn;
    return vec2_scalarMult(
            vec2_sub(point, vn->view_origin),
            vn->view_scale);
}

void canvasToScreenN(Vec2 *dest, Vec2 *src, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dest[i] = canvasToScreen(src[i]);
    }
}

Vec2 screenToCanvas(Vec2 point) {
    VnCtx *vn = &g_vn;
    return vec2_add(
            vec2_scalarMult(point, 1/vn->view_scale),
            vn->view_origin);
}

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
                if (vn->path_cnt == 0) break;
                Path *p = vn->paths[vn->path_cnt-1];
                for (size_t i = 0; i < p->node_cnt; i++) {
                    printf("{%f, %f},\n", p->nodes[i].x, p->nodes[i].y);
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

    // Handle right mouse button for panning
    if (vn->mouse_states[GLFW_MOUSE_BUTTON_RIGHT] == GLFW_PRESS) {
        Vec2 r = vec2_sub(
                screenToCanvas(vn->mouse_pos), vn->mouse_pos_rc);
        vn->view_origin.x += -r.x;
        vn->view_origin.y += -r.y;
    }

    Tool *tool = vn->tools[vn->active_tool];
    if (tool && tool->mousePosCb)
        tool->mousePosCb(tool, &vn->mouse_pos, vn->mouse_states);
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    VnCtx *vn = &g_vn;

    vn->mouse_states[button] = action;

    // Needed for panning
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            vn->mouse_pos_rc = screenToCanvas(vn->mouse_pos);
        }
    }

    Tool *tool = vn->tools[vn->active_tool];
    if (tool && tool->mouseBtnCb)
        tool->mouseBtnCb(tool, &vn->mouse_pos, button, action);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    VnCtx *vn = &g_vn;

    // Zoom by changing the scale parameter, and correcting the view_offset to
    // scale around the mouse position.
    if (yoffset != 0.0) {
        const double MAX_ZOOM_IN = 1.5e13;
        const double MAX_ZOOM_OUT = 1.0e-18;
        if (yoffset == 1.0 && vn->view_scale >= MAX_ZOOM_IN)
            return;
        if (yoffset == -1.0 && vn->view_scale <= MAX_ZOOM_OUT)
            return;

        Vec2 mouse_before = screenToCanvas(vn->mouse_pos);

        const double SCALING_FACTOR = 1.05;
        vn->view_scale *= yoffset > 0 ? SCALING_FACTOR : 1/SCALING_FACTOR;

        Vec2 mouse_after = screenToCanvas(vn->mouse_pos);

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

    vn->paths = malloc(DEFAULT_PATH_CAPACITY * sizeof(Path*));
    vn->path_capacity = DEFAULT_PATH_CAPACITY;

    assert(vn->paths != NULL);

    return vn;
}

void vn_deinit(VnCtx *vn) {
    for (size_t i = 0; i < sizeof(vn->shaders)/sizeof(GLuint); i++) {
        glDeleteProgram(vn->shaders[i]);
    }
    glDeleteBuffers(sizeof(vn->vbos)/sizeof(GLuint), vn->vbos);
    glDeleteVertexArrays(sizeof(vn->vaos)/sizeof(GLuint), vn->vaos);

    if (vn->paths) {
        for (size_t i = 0; i < vn->path_cnt; i++) {
            assert(vn->paths[i]);
            path_deinit(vn->paths[i]);
        }
    }

    if (vn->vg)
        nvgDeleteGL3(vn->vg);

    glfwDestroyWindow(vn->window);
    glfwTerminate();

    //free(vn);
}

// TODO: tmp
extern Path *dbg;

void vn_update(VnCtx *vn) {
    Tool *tool = vn->tools[vn->active_tool];
    Path *path = tool->update(tool, vn->view_scale);

    if (path) {
        if (vn->path_cnt >= vn->path_capacity) {
            // Path array is full, increase its capacity
            vn->path_capacity *= 2;
            vn->paths = realloc(vn->paths, vn->path_capacity * sizeof(Path*));
            assert(vn->paths != NULL);
        }

        vn->paths[vn->path_cnt] = path;

        printf("New path finished, %ld nodes, total %ld paths\n", vn->paths[vn->path_cnt]->node_cnt, vn->path_cnt);

        vn->path_cnt += 1;
    }

    NVGcontext *vg = vn->vg;

    nvgBeginFrame(vg, vn->view_width, vn->view_height, 1.0);
    nvgSave(vg);
    {
        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_MITER);
        nvgStrokeWidth(vg, 2.0f);
        nvgStrokeColor(vg, nvgRGBA(82, 144, 242, 255));

        if (tool->tmp_path && tool->tmp_path->node_cnt >= 2) {
            vn_drawLines(vn, tool->tmp_path);
        }

        for (size_t i = 0; i < vn->path_cnt; i++) {
            vn_drawPath(vn, vn->paths[i]);
        }
    }
    nvgRestore(vg);
    nvgEndFrame(vg);

    if (vn->debug) {
        //vn_drawCtrlPoints(vn, new);

        for (size_t i = 0; i < vn->path_cnt; i++) {
            vn_drawCtrlPoints(vn, vn->paths[i]);
        }

        {
            Rgb rgb = {255.0f/255, 200.0f/255, 64.0f/255};
            vn_drawDbgLines(vn, dbg->nodes, dbg->node_cnt, rgb, 1.0);
        }
    }
}

void vn_drawPath(VnCtx *vn, Path *path) {
    assert(vn->vg != NULL);

    nvgBeginPath(vn->vg);
    nvgStrokeColor(vn->vg, nvgRGBA(230, 20, 15, 255));

    Vec2 p = canvasToScreen(path->nodes[0]);
    nvgMoveTo(vn->vg, p.x, p.y);
    for (size_t j = 1; j < path->node_cnt; j+=3) {
        Vec2 p0 = canvasToScreen(path->nodes[j]);
        Vec2 p1 = canvasToScreen(path->nodes[j+1]);
        Vec2 p2 = canvasToScreen(path->nodes[j+2]);

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

    Vec2 p = canvasToScreen(path->nodes[0]);
    nvgMoveTo(vn->vg, p.x, p.y);
    for (size_t i = 1; i < path->node_cnt; i++) {
        p = canvasToScreen(path->nodes[i]);
        nvgLineTo(vn->vg, p.x, p.y);
    }
    nvgStroke(vn->vg);
}

void vn_drawCtrlPoints(VnCtx *vn, Path *path) {
    GLuint color_loc;

    Vec2 *nodes = malloc(sizeof(Vec2) * path->node_cnt);
    canvasToScreenN(nodes, path->nodes, path->node_cnt);

    glBindVertexArray(vn->vaos[VAO_spline]);

    glBindBuffer(GL_ARRAY_BUFFER, vn->vbos[VBO_spline]);
    glBufferData(GL_ARRAY_BUFFER, path->node_cnt*sizeof(Vec2), nodes, GL_DYNAMIC_DRAW);

    {
        glUseProgram(vn->shaders[SHADER_simple]);
        glPointSize(4.0f);
        color_loc = glGetUniformLocation(vn->shaders[SHADER_simple], "color");

        //glUniform4f(color_loc, 0.43, 0.43, 0.43, 1.0);
        glUniform4f(color_loc, 0.60, 0.60, 0.60, 1.0);
        glDrawArrays(GL_POINTS, 0, path->node_cnt);
    }


    {
        glUseProgram(vn->shaders[SHADER_stipple]);
        glEnable(GL_LINE_SMOOTH);
        glLineWidth(1.0f);
        color_loc = glGetUniformLocation(vn->shaders[SHADER_stipple], "color");

        glUniform4f(color_loc, 0.173, 0.325, 0.749, 1.0);
        glDrawArrays(GL_LINE_STRIP, 0, path->node_cnt);
    }

    free(nodes);
}

void vn_drawDbgLines(VnCtx *vn, Vec2 *points, size_t count, Rgb color, float linewidth) {
    Vec2 *p = malloc(sizeof(Vec2) * count);
    canvasToScreenN(p, points, count);

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
