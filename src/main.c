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

typedef enum path_type {
    PATHTYPE_line,
    PATHTYPE_bezier,
} PathType;

typedef struct ui {
    GLuint vbo[VBO_count];
    GLuint vao[VAO_count];
    GLuint shader[SHADER_count];

    Vec2 mouse_pos;
    int mouse_button, mouse_action;

    unsigned int width, height;
} UI;

#define PATH_MAX_NODES 128 * 4
typedef struct path {
    PathType    type;
    Vec2        nodes[PATH_MAX_NODES];
    double      timestamps[PATH_MAX_NODES];
    size_t      node_cnt;

    PathCmd     commands[PATH_MAX_NODES];
} Path;

UI ui;
Path *g_path;
Path *dbg;

Path* path_init() {
    Path path = { 0 };
    return calloc(1, sizeof(path));
}

void path_deinit(Path *path) {
    free(path);
}

void path_addNode(Path *path, Vec2 node, double timestamp) {
    assert(path->node_cnt < PATH_MAX_NODES);
    path->nodes[path->node_cnt] = node;
    path->timestamps[path->node_cnt] = timestamp < 0 ? glfwGetTime() : timestamp;
    path->node_cnt += 1;
}

Vec2* path_getNode(Path *path, int index) {
    int pos = index < 0 ? path->node_cnt + index : index;
    if (pos < (int)path->node_cnt) {
        return &path->nodes[pos];
    }
    return NULL;
}

typedef struct bezier_coeffs {
    double B0;
    double B1;
    double B2;
    double B3;

    double dB0;
    double dB1;
    double dB2;

    double ddB0;
    double ddB1;
} BezierCoeffs;

typedef struct bezier_fit_ctx {
    size_t  count;

    Vec2    *points;
    double  *params;

    BezierCoeffs *coeffs;

    double epsilon;
    double psi;
    unsigned max_iter;
} BezierFitCtx;

BezierFitCtx *initCtx(Vec2 points[], size_t count) {
    BezierFitCtx *fit = malloc(sizeof(BezierFitCtx));
    fit->count = count;
    fit->points = points;
    fit->params = malloc(sizeof(double) * count);
    fit->coeffs = malloc(sizeof(BezierCoeffs) * count);

    // Sane defaults
    fit->epsilon = 8.0;
    fit->psi = 30.0;
    fit->max_iter = 3;

    return fit;
}

void deinitCtx(BezierFitCtx *fit) {
    free(fit->params);
    free(fit->coeffs);
    free(fit);
}

void calcCoefficients(BezierFitCtx *fit, size_t i_start, size_t i_end) {
    assert(i_end < fit->count);
    for (size_t i = i_start; i <= i_end; i++) {
        BezierCoeffs *c = &fit->coeffs[i];
        double u = fit->params[i];

        double omu = 1-u;

        c->B0 = omu*omu*omu;
        c->B1 = 3*u * omu*omu;
        c->B2 = 3*u*u * omu;
        c->B3 = u*u*u;

        c->dB0 = 3 * omu*omu;
        c->dB1 = 6*u * omu;
        c->dB2 = 3*u*u;

        c->ddB0 = 6 * omu;
        c->ddB1 = 6*u;
    }
}

void chordLengthParameterization(BezierFitCtx *fit, size_t i_start, size_t i_end) {
    fit->params[i_start] = 0.0;
    for (size_t i = i_start+1; i <= i_end; i++) {
        fit->params[i] = fit->params[i-1] + vec2_dist(fit->points[i], fit->points[i-1]);
    }
    for (size_t i = i_start+1; i <= i_end; i++) {
        fit->params[i] = fit->params[i] / fit->params[i_end];
    }
}

void reparameterize(BezierFitCtx *fit, Vec2 v0, Vec2 v1, Vec2 v2, Vec2 v3, size_t i_start, size_t i_end) {
    assert(i_end < fit->count);
    for (size_t i = i_start; i <= i_end; i++) {
        double u = fit->params[i];
        Vec2 d = fit->points[i];

        double B0 = fit->coeffs[i].B0;
        double B1 = fit->coeffs[i].B1;
        double B2 = fit->coeffs[i].B2;
        double B3 = fit->coeffs[i].B3;

        double dB0 = fit->coeffs[i].dB0;
        double dB1 = fit->coeffs[i].dB1;
        double dB2 = fit->coeffs[i].dB2;

        double ddB0 = fit->coeffs[i].dB0;
        double ddB1 = fit->coeffs[i].dB1;

        Vec2 Q = vec2_scalarMult(v0, B0);
        Q = vec2_add(Q, vec2_scalarMult(v1, B1));
        Q = vec2_add(Q, vec2_scalarMult(v2, B2));
        Q = vec2_add(Q, vec2_scalarMult(v3, B3));

        Vec2 dQ = vec2_scalarMult(vec2_sub(v1, v0), dB0);
        dQ = vec2_add(dQ, vec2_scalarMult(vec2_sub(v2, v1), dB1));
        dQ = vec2_add(dQ, vec2_scalarMult(vec2_sub(v3, v2), dB2));

        Vec2 ddQ = vec2_scalarMult(
                vec2_add(
                    vec2_sub(v2, vec2_scalarMult(v1, 2)),
                    v0),
                ddB0);

        ddQ = vec2_add(ddQ, vec2_scalarMult(
                vec2_add(
                    vec2_sub(v3, vec2_scalarMult(v1, 2)),
                    v1),
                ddB1));

        double num = vec2_dot(vec2_sub(Q, d), dQ);
        double denom = vec2_dot(dQ, dQ) + vec2_dot(vec2_sub(Q, d), ddQ);
        fit->params[i] = u - (num / denom);
    }
    assert(fit->params[i_start] == 0);
    assert(fit->params[i_end] == 1.00);
}

Vec2 calcBezier(BezierFitCtx *fit, unsigned index, Vec2 v0, Vec2 v1, Vec2 v2, Vec2 v3) {
    Vec2 p = vec2_scalarMult(v0, fit->coeffs[index].B0);
    p = vec2_add(p, vec2_scalarMult(v1, fit->coeffs[index].B1));
    p = vec2_add(p, vec2_scalarMult(v2, fit->coeffs[index].B2));
    p = vec2_add(p, vec2_scalarMult(v3, fit->coeffs[index].B3));
    return p;
}

// TODO still:
//  - Map t0 and tend to the final path.
void fitBezier(BezierFitCtx *fit, Path* new, Vec2 t1, Vec2 t2, unsigned level, size_t i_start, size_t i_end) {
    printf("fitBezier called; i_start=%ld, i_end=%ld\n", i_start, i_end);

    assert(i_end < fit->count);
    Vec2 v0 = fit->points[i_start];
    Vec2 v3 = fit->points[i_end];

    if (i_end - i_start == 1) {
        // Only two points

        double dist = vec2_dist(v0, v3) / 3.0;
        printf("Only two points! %f\n", dist);
        path_addNode(new, vec2_add(v0, vec2_scalarMult(t1, dist)), 0);
        path_addNode(new, vec2_add(v3, vec2_scalarMult(t2, dist)), 0);
        path_addNode(new, v3, fit->params[i_end]);
        return;
    }

    double c11, c1221, c22, x1, x2;
    c11 = c1221 = c22 = x1 = x2 = 0;

    for (size_t i = i_start; i <= i_end; i++) {
        Vec2 d = fit->points[i];

        double B1 = fit->coeffs[i].B1;
        double B2 = fit->coeffs[i].B2;

        Vec2 A1 = vec2_scalarMult(t1, B1);
        Vec2 A2 = vec2_scalarMult(t1, B2);

        c11 += vec2_dot(A1, A1);
        c1221 += vec2_dot(A1, A2);
        c22 += vec2_dot(A2, A2);

        Vec2 bisum = calcBezier(fit, i, v0, v0, v3, v3);

        Vec2 sub = vec2_sub(d, bisum);
        x1 += vec2_dot(sub, A1);
        x2 += vec2_dot(sub, A2);
    }

    double a1 = (x1*c22 - c1221*x2) / (c11*c22 - c1221*c1221);
    double a2 = (c11*x2 - x1*c1221) / (c11*c22 - c1221*c1221);

    Vec2 v1 = vec2_add(v0, vec2_scalarMult(t1, a1));
    Vec2 v2 = vec2_add(v3, vec2_scalarMult(t2, a2));
    //printf("a1=%f, a2=%f\n", a1, a2);

    // TODO: Store the calculated parameters in an array for reuse.
    double max_err = 0;
    double max_err_t = 0;
    size_t max_err_i = 0;
    Vec2 max_err_d = {0, 0};
    for (size_t i = i_start; i <= i_end; i++) {
        double t = fit->params[i]; // Only needed for Bn calcs, so can be removed now
        Vec2 d = fit->points[i];

        Vec2 p = calcBezier(fit, i, v0, v1, v2, v3);

        double err = vec2_distSqr(d, p);
        if (err > max_err) {
            max_err = err;
            max_err_t = t;
            max_err_i = i;
            max_err_d = d;
        }
    }
    printf("Level %d; Max distance error is %f at t=%f\n", level, sqrt(max_err), max_err_t);

    if (max_err > fit->psi*fit->psi) {
        // Error is very large, split the curve into multiple paths and try on
        // these paths separately.

        printf("Splitting! err=%f, i=%ld\n", sqrt(max_err), max_err_i);

        Vec2 t_split = vec2_norm(vec2_sub(fit->points[max_err_i-1], fit->points[max_err_i+1]));

        chordLengthParameterization(fit, i_start, max_err_i);
        calcCoefficients(fit, i_start, max_err_i);
        fitBezier(fit, new, t1, t_split, level, i_start, max_err_i);

        t_split = vec2_scalarMult(t_split, -1);
        chordLengthParameterization(fit, max_err_i, i_end);
        calcCoefficients(fit, max_err_i, i_end);
        fitBezier(fit, new, t_split, t2, level, max_err_i, i_end);
        return;
    } else if (max_err > fit->epsilon*fit->epsilon && level < fit->max_iter) {
        // The error is fairly small but still too large, try to improve by
        // reparameterizing.

        calcCoefficients(fit, i_start, i_end);
        reparameterize(fit, v0, v1, v2, v3, i_start, i_end);
        fitBezier(fit, new, t1, t2, level+1, i_start, i_end);
        return;
    };
    // Error is small enough; continue

    Vec2 p = vec2_scalarMult(v0, fit->coeffs[max_err_i].B0);
    p = vec2_add(p, vec2_scalarMult(v1, fit->coeffs[max_err_i].B1));
    p = vec2_add(p, vec2_scalarMult(v2, fit->coeffs[max_err_i].B2));
    p = vec2_add(p, vec2_scalarMult(v3, fit->coeffs[max_err_i].B3));
    path_addNode(dbg, max_err_d, 0);
    path_addNode(dbg, p, 0);

    //printf("{%f, %f}, {%f, %f},\n", v1.x, v1.y, v2.x, v2.y);
    path_addNode(new, v1, -1);
    path_addNode(new, v2, -1);
    path_addNode(new, v3, fit->params[i_end]);
}

Path* fitPath(Path *path, double epsilon, double psi, int max_iter) {
    assert(path->node_cnt > 2);

    BezierFitCtx *fit = initCtx(path->nodes, path->node_cnt);
    fit->epsilon = epsilon;
    fit->psi = psi;
    fit->max_iter = max_iter;

    // TODO: Proper initial parameterization
    //       Keep in mind that I want to tag the final points with real life
    //       timestamps (derived from path->timestamps).
    //       Check if it would add anything to use real timestamps instead of
    //       chord-length initialization.
    //double *params = fit->params;
    //double u0 = path->timestamps[0];
    //double uend = path->timestamps[path->node_cnt-1];
    //for (size_t i = 0; i < path->node_cnt; i++) {
    //    params[i] = (path->timestamps[i] - u0) / (uend - u0);
    //}

    Path *new = path_init();
    path_addNode(new, fit->points[0], fit->params[0]); // params[0] should always be 0

    // TODO: See if the tangent can be better approximated
    Vec2 t1 = vec2_norm(vec2_sub(fit->points[1], fit->points[0]));
    Vec2 t2 = vec2_norm(vec2_sub(fit->points[fit->count-2], fit->points[fit->count-1]));

    chordLengthParameterization(fit, 0, fit->count-1);
    calcCoefficients(fit, 0, fit->count-1);
    fitBezier(fit, new, t1, t2, 0, 0, fit->count-1);

    deinitCtx(fit);

    return new;
}

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

void ui_drawDbgLines(UI *ui, Vec2 *points, size_t count, Rgb color, float linewidth) {
    glUseProgram(ui->shader[SHADER_debug]);
    glBindVertexArray(ui->vao[VAO_debug]);

    glBindBuffer(GL_ARRAY_BUFFER, ui->vbo[VBO_debug]);
    glBufferData(GL_ARRAY_BUFFER, count*sizeof(Vec2), points, GL_DYNAMIC_DRAW);

    GLuint color_loc = glGetUniformLocation(ui->shader[SHADER_debug], "color");
    glUniform3f(color_loc, color.r, color.g, color.b);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(linewidth);
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
            prev_node = path_getNode(g_path, -1);
        }

        Vec2 curr = {
            .x = xpos,
            .y = ypos,
        };

        double cmp = 5.0;
        double curr_len = 0;
        if (g_path->node_cnt > 1) {
            // The last two points, and a tangent vector determined from these
            // points.
            Vec2 p1 = g_path->nodes[g_path->node_cnt-1];
            Vec2 p0 = g_path->nodes[g_path->node_cnt-2];
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
            path_addNode(g_path, curr, -1);

            prev_node = path_getNode(g_path, -1);
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

        path_addNode(g_path, node, -1);
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
    dbg = path_init();

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

        //{465.000000, 323.000000},
        //{463.000000, 313.000000},
        //{461.000000, 303.000000},
        //{459.000000, 293.000000},
        //{457.000000, 283.000000},
        //{457.000000, 272.000000},
        //{457.000000, 260.000000},
        //{457.000000, 249.000000},
        //{458.000000, 239.000000},
        //{460.000000, 229.000000},
        //{463.000000, 219.000000},
        //{467.000000, 209.000000},
        //{472.000000, 199.000000},
        //{479.000000, 191.000000},
        //{487.000000, 183.000000},
        //{496.000000, 177.000000},
        //{507.000000, 173.000000},
        //{517.000000, 171.000000},
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
        path_addNode(g_path, test[i], -1);
    }

    Path *new = fitPath(g_path, 5.0, 30.0, 4);
    printf("New has %ld items\n", new->node_cnt);

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
        node = &g_path->nodes[0];

        nvgLineCap(vg, NVG_ROUND);
        nvgLineJoin(vg, NVG_MITER);
        nvgStrokeWidth(vg, 2.0f);
        nvgStrokeColor(vg, nvgRGBA(82, 144, 242, 255));

        nvgBeginPath(vg);
        nvgMoveTo(vg, node->x, node->y);
        for (size_t i = 1; i < g_path->node_cnt; i++) {
            node = &g_path->nodes[i];
            nvgLineTo(vg, node->x, node->y);
        }
        nvgStroke(vg);

        nvgBeginPath(vg);
        nvgStrokeColor(vg, nvgRGBA(230, 20, 15, 255));
        nvgMoveTo(vg, new->nodes[0].x, new->nodes[0].y);
        for (size_t i = 1; i < new->node_cnt; i+=3) {
            nvgBezierTo(vg,
                    new->nodes[i].x, new->nodes[i].y,
                    new->nodes[i+1].x, new->nodes[i+1].y,
                    new->nodes[i+2].x, new->nodes[i+2].y);
        }
        nvgStroke(vg);

        nvgRestore(vg);
        nvgEndFrame(vg);

        //ui_drawSpline(&ui, g_path);

        if (g_path->node_cnt > 1) {
            Vec2 p1 = g_path->nodes[g_path->node_cnt-1];
            Vec2 p0 = g_path->nodes[g_path->node_cnt-2];
            Vec2 tg = vec2_norm(vec2_sub(p1, p0));

            Vec2 points[] = {
                p1,
                vec2_add(p1, vec2_scalarMult(tg, 30.0)),
            };
            Rgb rgb = {1.0, 1.0, 1.0};

            ui_drawDbgLines(&ui, points, 2, rgb, 1.0f);
        }

        {
            Rgb rgb = {1, 1, 1};
            ui_drawDbgLines(&ui, new->nodes, 6, rgb, 1.0f);
        }
        {
            Rgb rgb = {255.0f/255, 200.0f/255, 64.0f/255};
            ui_drawDbgLines(&ui, dbg->nodes, dbg->node_cnt, rgb, 3.0);
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
    path_deinit(g_path);
    path_deinit(new);

    glfwTerminate();
    return 0;
}
