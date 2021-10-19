// Next up: Determine rate at which the direction is changing. Calc the current
// tangent vector and project the new r vector on it. Use that projection as
// some tolerance / weight for how frequent points should be placed. (e.g. If
// projection is 100%, only place every x pixels. If projection is only 60% or
// so, place very frequently as we are probably going around a curve right now)

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "nanovg/nanovg.h"
#define NANOVG_GL3_IMPLEMENTATION
#include "nanovg/nanovg_gl.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>

#include "vec.h"
#include "gl.h"

const double PI = 3.1415926535897932384626433832795;
const double PI_2 = 1.57079632679489661923;

#define WIDTH 800
#define HEIGHT 600

enum vbo_type {
    VBO_spline,
    VBO_count,
};

enum vao_type {
    VAO_spline,
    VAO_count,
};

enum shader_type {
    SHADER_spline,
    SHADER_debug,
    SHADER_count,
};

typedef enum path_cmd {
    PATHCMD_end,
    PATHCMD_line_to,
    PATHCMD_count,
} PathCmd;

#define PATH_MAX_NODES 128 * 4
typedef struct path {
    Vec2    nodes[PATH_MAX_NODES];
    size_t  node_cnt;

    PathCmd commands[PATH_MAX_NODES];
} Path;

Path g_path;

Path path_init() {
    Path path = { 0 };
    return path;
}

void path_addNode(Path *path, Vec2 node) {
    assert(path->node_cnt < PATH_MAX_NODES);
    path->nodes[path->node_cnt++] = node;
}

Vec2* path_getNode(Path *path, int index) {
    int pos = index < 0 ? path->node_cnt + index : index;
    if (pos < (int)path->node_cnt) {
        return &path->nodes[pos];
    }
    return NULL;
}

typedef struct ui {
    GLuint vbo[VBO_count];
    GLuint vao[VAO_count];
    GLuint shader[SHADER_count];

    Vec2 mouse_pos;
    int mouse_button, mouse_action;

    unsigned int width, height;
} UI;

UI ui;

UI ui_init() {
    UI ui = { 0 };

    glGenVertexArrays(VAO_count, ui.vao);
    glGenBuffers(VBO_count, ui.vbo);
    for (int i = 0; i < VAO_count; i++) {
        assert(i < VBO_count);
        glBindVertexArray(ui.vao[i]);
        glBindBuffer(GL_ARRAY_BUFFER, ui.vbo[i]);

        switch (i) {
        case VAO_spline:
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
            //{ GL_TESS_CONTROL_SHADER, true, "glsl/bezier.tcs" },
            //{ GL_TESS_EVALUATION_SHADER, true, "glsl/bezier.tes" },
            //{ GL_GEOMETRY_SHADER, true, "glsl/stroke.gs" },
            { GL_FRAGMENT_SHADER, true, "glsl/simple.fs" },
            { GL_NONE },
        };

        ui.shader[SHADER_spline] = gl_createProgram(shaders);
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
        ui.shader[SHADER_debug] = gl_createProgram(shaders);
    }

    return ui;
}

void ui_drawSpline(UI *ui, Path *path) {
    glUseProgram(ui->shader[SHADER_spline]);
    glBindVertexArray(ui->vao[VAO_spline]);

    glBindBuffer(GL_ARRAY_BUFFER, ui->vbo[VBO_spline]);
    glBufferData(GL_ARRAY_BUFFER, path->node_cnt*sizeof(Vec2), path->nodes, GL_DYNAMIC_DRAW);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
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

#define NUM_MOUSE_STATES 8
int g_mouse_states[NUM_MOUSE_STATES] = {0};
double g_xpos, g_ypos;

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    g_xpos = xpos;
    g_ypos = ypos;

    static Vec2 *prev_node = NULL;

    if (g_mouse_states[GLFW_MOUSE_BUTTON_LEFT] == GLFW_PRESS) {
        if (!prev_node) {
            prev_node = path_getNode(&g_path, -1);
        }

        Vec2 curr = {
            .x = xpos,
            .y = ypos,
        };

        if (vec2_distSqr(*prev_node, curr) > 100) {
            //printf("Placing new node! %f %f\n", xpos, ypos);
            printf("{%f, %f}\n", xpos, ypos);

            path_addNode(&g_path, curr);

            prev_node = path_getNode(&g_path, -1);
        }
    } else {
        // Mouse not held
        prev_node = NULL;
    }
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    g_mouse_states[button] = action;

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        Vec2 node = {
            .x = g_xpos,
            .y = g_ypos,
        };

        path_addNode(&g_path, node);
        printf("Added a node at %f %f\n", g_xpos, g_ypos);
    }
}

static void set_viewport(int width, int height) {
    glViewport(0, 0, width, height);

    // TODO: Get rid of ui in global, and find a clean way to pass the view size
    // to the shader programs
    glUniform2f(glGetUniformLocation(ui.shader[SHADER_spline], "view_size"), width, height);
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    set_viewport(width, height);
}

static void glfwError(int id, const char* desc) {
    printf("Error(GLFW): %s\n", desc);
}

int main(void) {
    g_path = path_init();

    glfwSetErrorCallback(&glfwError);
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window and context
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "VectorNotes", NULL, NULL);
    if (window == NULL) {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Prepare GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    // Set viewport size, and callback function for when the size changes
    glViewport(0, 0, WIDTH, HEIGHT);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Configure user input callbacks
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    Vec2 test[] = {
        //{400.0, 200.0},
        //{200.0, 250.0},
        //{220.0, 400.0},
        //{400.0, 350.0},

        {465.000000, 323.000000},
        {463.000000, 313.000000},
        {461.000000, 303.000000},
        {459.000000, 293.000000},
        {457.000000, 283.000000},
        {457.000000, 272.000000},
        {457.000000, 260.000000},
        {457.000000, 249.000000},
        {458.000000, 239.000000},
        {460.000000, 229.000000},
        {463.000000, 219.000000},
        {467.000000, 209.000000},
        {472.000000, 199.000000},
        {479.000000, 191.000000},
        {487.000000, 183.000000},
        {496.000000, 177.000000},
        {507.000000, 173.000000},
        {517.000000, 171.000000},
        {529.000000, 171.000000},
        {539.000000, 172.000000},
        {549.000000, 175.000000},
        {559.000000, 179.000000},
        {570.000000, 183.000000},
        {580.000000, 188.000000},
        {591.000000, 194.000000},
        {600.000000, 199.000000},
        {609.000000, 205.000000},
        {618.000000, 211.000000},
        {626.000000, 218.000000},
        {634.000000, 226.000000},
        {641.000000, 234.000000},
        {647.000000, 243.000000},
        {652.000000, 252.000000},
        {657.000000, 262.000000},
        {661.000000, 273.000000},
        {663.000000, 283.000000},
        {665.000000, 293.000000},
        {666.000000, 303.000000},
        {667.000000, 313.000000},
        {667.000000, 324.000000},
        {667.000000, 335.000000},
        {665.000000, 345.000000},
    };

    for (size_t i = 0; i < sizeof(test) / sizeof(Vec2); i++) {
        path_addNode(&g_path, test[i]);
    }

    ui = ui_init();

    NVGcontext *vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (!vg) {
        return -2;
    }

    glfwSetTime(0);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        nvgBeginFrame(vg, WIDTH, HEIGHT, 1.0);
        nvgSave(vg);

        nvgRestore(vg);
        nvgEndFrame(vg);

        ui_drawSpline(&ui, &g_path);

        // TODO: Adjacency is currently hacked in by setting the last+1 element
        // equal to the last. This will buffer overflow.
        //glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, 4);
        //glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, g_path.node_cnt+1);

        glEnable(GL_LINE_SMOOTH);
        glPointSize(5.0f);
        glDrawArrays(GL_POINTS, 0, g_path.node_cnt);
        glDrawArrays(GL_LINE_STRIP, 0, g_path.node_cnt);

        //glEnable(GL_LINE_SMOOTH);
        //glLineWidth(16.0f);
        //glPatchParameteri(GL_PATCH_VERTICES, 4);
        //glDrawArrays(GL_PATCHES, 0, 4);

        //glEnable(GL_LINE_SMOOTH);
        //glLineWidth(5.0f);
        //glDrawArrays(GL_LINE_STRIP, 0, g_path.node_cnt);

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.016666);
    }

    glfwTerminate();
    return 0;
}
