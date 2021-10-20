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
    double  timestamps[PATH_MAX_NODES];
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
    path->nodes[path->node_cnt] = node;
    path->timestamps[path->node_cnt] = glfwGetTime();

    printf("{%f, %f} t=%f\n", node.x, node.y, path->timestamps[path->node_cnt]);


    path->node_cnt += 1;
}

Vec2* path_getNode(Path *path, int index) {
    int pos = index < 0 ? path->node_cnt + index : index;
    if (pos < (int)path->node_cnt) {
        return &path->nodes[pos];
    }
    return NULL;
}

void fitBezier(Vec2 t1, Vec2 t2, double params[], Vec2 *points, size_t count, double epsilon) {
    Vec2 v0 = points[0];
    Vec2 v3 = points[count-1];

    double u0 = params[0];
    double uend = params[count-1];

    double c11, c1221, c22, x1, x2;
    c11 = c1221 = c22 = x1 = x2 = 0;

    for (size_t i = 0; i < count; i++) {
        double t = (params[i] - u0) / (uend - u0);
        Vec2 d = points[i];

        double B0 = (1-t) * (1-t) * (1-t);
        double B1 = 3*t * (1-t) * (1-t);
        double B2 = 3*t*t * (1-t);
        double B3 = t*t*t;

        Vec2 A1 = vec2_scalarMult(t1, B1);
        Vec2 A2 = vec2_scalarMult(t1, B2);

        c11 += vec2_dot(A1, A1);
        c1221 += vec2_dot(A1, A2);
        c22 += vec2_dot(A2, A2);

        Vec2 bisum = vec2_scalarMult(v0, B0);
        bisum = vec2_add(bisum, vec2_scalarMult(v0, B1));
        bisum = vec2_add(bisum, vec2_scalarMult(v3, B2));
        bisum = vec2_add(bisum, vec2_scalarMult(v3, B3));

        Vec2 sub = vec2_sub(d, bisum);
        x1 += vec2_dot(sub, A1);
        x2 += vec2_dot(sub, A2);
    }

    double a1 = (x1*c22 - c1221*x2) / (c11*c22 - c1221*c1221);
    double a2 = (c11*x2 - x1*c1221) / (c11*c22 - c1221*c1221);

    Vec2 v1 = vec2_add(v0, vec2_scalarMult(t1, a1));
    Vec2 v2 = vec2_add(v3, vec2_scalarMult(t2, a2));

    // TODO: Store the calculated parameters in an array for reuse.
    double max_err = 0;
    double max_err_t = 0;
    for (size_t i = 0; i < count; i++) {
        double t = (params[i] - u0) / (uend - u0);
        Vec2 d = points[i];

        double B0 = (1-t) * (1-t) * (1-t);
        double B1 = 3*t * (1-t) * (1-t);
        double B2 = 3*t*t * (1-t);
        double B3 = t*t*t;

        Vec2 p = vec2_scalarMult(v0, B0);
        p = vec2_add(p, vec2_scalarMult(v1, B1));
        p = vec2_add(p, vec2_scalarMult(v2, B2));
        p = vec2_add(p, vec2_scalarMult(v3, B3));

        double err = vec2_dist(d, p);
        if (err > max_err) {
            max_err = err;
            max_err_t = t;
        }

        printf("Err at t=%f \t: %f\n", t, err);

        //if (t == 0.375532) {
        if (t >= 0.345530 && t <= 0.375535) {
            printf(">>> orig: {%f, %f}, est: {%f, %f},\n", d.x, d.y, p.x, p.y);
        }
    }
    printf("Max distance error is %f at t=%f\n", max_err, max_err_t);

    printf("{%f, %f}, {%f, %f},\n", v1.x, v1.y, v2.x, v2.y);
}

void fitPath(Path *path, double epsilon) {
    assert(path->node_cnt > 2);

    Vec2 t1 = vec2_norm(vec2_sub(path->nodes[1], path->nodes[0]));
    Vec2 t2 = vec2_norm(vec2_sub(path->nodes[path->node_cnt-1], path->nodes[path->node_cnt-2]));
    fitBezier(t1, t2, path->timestamps, path->nodes, path->node_cnt, epsilon);
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

    GLuint color_loc = glGetUniformLocation(ui->shader[SHADER_spline], "color");

    glEnable(GL_LINE_SMOOTH);
    glPointSize(4.0f);
    glLineWidth(2.0f);

    glUniform3f(color_loc, 0.173, 0.325, 0.749);
    glDrawArrays(GL_LINE_STRIP, 0, path->node_cnt);
    glUniform3f(color_loc, 0.70, 0.70, 0.70);
    glDrawArrays(GL_POINTS, 0, path->node_cnt);
}

void ui_drawDbgLines(UI *ui, Vec2 *points, size_t count, Rgb color) {
    glUseProgram(ui->shader[SHADER_debug]);
    glBindVertexArray(ui->vao[VAO_debug]);

    glBindBuffer(GL_ARRAY_BUFFER, ui->vbo[VBO_debug]);
    glBufferData(GL_ARRAY_BUFFER, count*sizeof(Vec2), points, GL_DYNAMIC_DRAW);

    GLuint color_loc = glGetUniformLocation(ui->shader[SHADER_debug], "color");
    glUniform3f(color_loc, color.r, color.g, color.b);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, count);
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
    static double prev_len = 0;

    if (g_mouse_states[GLFW_MOUSE_BUTTON_LEFT] == GLFW_PRESS) {
        if (!prev_node) {
            prev_node = path_getNode(&g_path, -1);
        }

        Vec2 curr = {
            .x = xpos,
            .y = ypos,
        };

        double cmp = 5.0;
        double curr_len = 0;
        if (g_path.node_cnt > 1) {
            // The last two points, and a tangent vector determined from these
            // points.
            Vec2 p1 = g_path.nodes[g_path.node_cnt-1];
            Vec2 p0 = g_path.nodes[g_path.node_cnt-2];
            Vec2 tg = vec2_norm(vec2_sub(p1, p0));

            // Vector from the last node to the cursor position.
            Vec2 r = vec2_sub(curr, *prev_node);
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
            //double cmp = 100 / (1 + pow(128, 4*alpha - 2)) + 10;
            //double cmp = 100 * (1 - 1/(1 + pow(128, 100*alpha - 99.5))) + 8;
            cmp = max_len*pow(alpha, exponent) + min_len;

            // Whenever the cursor moves back, place a node to capture this movement
            if (curr_len > prev_len)
                prev_len = curr_len;
        }

        //if (fn < 0.7 && vec2_dist(*prev_node, curr) > 8) {
        if (curr_len < (prev_len - 5.0) || vec2_dist(*prev_node, curr) > cmp) {
            path_addNode(&g_path, curr);

            prev_node = path_getNode(&g_path, -1);
            prev_len = 0;
        }
    } else {
        // Mouse not held
        prev_node = NULL;
        prev_len = 0;
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
    glProgramUniform2f(ui.shader[SHADER_spline], glGetUniformLocation(ui.shader[SHADER_spline], "view_size"), width, height);
    glProgramUniform2f(ui.shader[SHADER_debug], glGetUniformLocation(ui.shader[SHADER_debug], "view_size"), width, height);
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    set_viewport(width, height);
    ui.width = width;
    ui.height = height;
}

static void glfwError(int id, const char* desc) {
    fprintf(stderr, "Error(GLFW): %s\n", desc);
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
        //{570.000000, 183.000000},
        //{580.000000, 188.000000},
        //{591.000000, 194.000000},
        //{600.000000, 199.000000},
        //{609.000000, 205.000000},
        //{618.000000, 211.000000},
        //{626.000000, 218.000000},
        //{634.000000, 226.000000},
        //{641.000000, 234.000000},
        //{647.000000, 243.000000},
        //{652.000000, 252.000000},
        //{657.000000, 262.000000},
        //{661.000000, 273.000000},
        //{663.000000, 283.000000},
        //{665.000000, 293.000000},
        //{666.000000, 303.000000},
        //{667.000000, 313.000000},
        //{667.000000, 324.000000},
        //{667.000000, 335.000000},
        //{665.000000, 345.000000},
    };

    for (size_t i = 0; i < sizeof(test) / sizeof(Vec2); i++) {
        path_addNode(&g_path, test[i]);
    }

    fitPath(&g_path, 0.10);

    NVGcontext *vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES | NVG_DEBUG);
    if (!vg) {
        return -2;
    }

    ui = ui_init();

    glfwSetTime(0);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        nvgBeginFrame(vg, ui.width, ui.height, 1.0);
        nvgSave(vg);

        Vec2 *node = NULL;
        node = &g_path.nodes[0];

        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_MITER);
        nvgStrokeWidth(vg, 2.0f);
        nvgStrokeColor(vg, nvgRGBA(82, 144, 242, 255));

        nvgBeginPath(vg);
        nvgMoveTo(vg, node->x, node->y);
        for (size_t i = 1; i < g_path.node_cnt; i++) {
            node = &g_path.nodes[i];
            nvgLineTo(vg, node->x, node->y);
        }
        nvgStroke(vg);

        nvgBeginPath(vg);
        nvgStrokeColor(vg, nvgRGBA(230, 20, 15, 255));
        nvgMoveTo(vg, g_path.nodes[0].x, g_path.nodes[0].y);
        //nvgBezierTo(vg, 440.31, 199.55, 714.42, 97.9,
        nvgBezierTo(vg, 471.4, 356, 474.0, 146.2,
                g_path.nodes[g_path.node_cnt-1].x, g_path.nodes[g_path.node_cnt-1].y);
        nvgStroke(vg);

        nvgRestore(vg);
        nvgEndFrame(vg);

        ui_drawSpline(&ui, &g_path);

        if (g_path.node_cnt > 1) {
            Vec2 p1 = g_path.nodes[g_path.node_cnt-1];
            Vec2 p0 = g_path.nodes[g_path.node_cnt-2];
            Vec2 tg = vec2_norm(vec2_sub(p1, p0));

            Vec2 points[] = {
                p1,
                vec2_add(p1, vec2_scalarMult(tg, 30.0)),
            };
            Rgb rgb = {1.0, 1.0, 1.0};

            ui_drawDbgLines(&ui, points, 2, rgb);
        }

        {
            Vec2 points[] = {
                test[0],
                //{444.292401, 219.462003}, {611.577620, 612.111898},
                //{432.355162, 159.775811}, {709.610034, 121.949829},
                {473.273050, 364.365251}, {643.173971, 212.669588},
                test[sizeof(test)/sizeof(Vec2) - 1],

                {463.000000, 219.000000}, {521.296420, 215.354138},
            };
            Rgb rgb = {1, 1, 1};
            ui_drawDbgLines(&ui, points, 6, rgb);
        }

        // TODO: Adjacency is currently hacked in by setting the last+1 element
        // equal to the last. This will buffer overflow.
        //glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, 4);
        //glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, g_path.node_cnt+1);

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
