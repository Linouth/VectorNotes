// https://stackoverflow.com/questions/68134348/draw-anti-aliased-thick-tessellated-curves
// Good: https://jcgt.org/published/0002/02/08/paper.pdf
//
// https://github.com/memononen/nanovg
// https://github.com/tyt2y3/vaserenderer
//
// https://computeranimations.wordpress.com/2015/03/16/rasterization-of-parametric-curves-using-tessellation-shaders-in-glsl/
// https://github.com/fcaruso/GLSLParametricCurve
//
// Geometry shaders for drawing strokes: https://gfx.cs.princeton.edu/pubs/Cole_2010_TFM/cole_tfm_preprint.pdf
//     - https://www.youtube.com/watch?v=RP1MVD4hAJM
//     - Apparently geometry shaders are slow(?). Tesselation shaders are
//     quicker.
//
// https://mattdesl.svbtle.com/drawing-lines-is-hard
//
// Generally people incrementally add data to VBO with glBufferSubData for
// something like this.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

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

typedef enum path_cmd {
    PATHCMD_line_to,
    PATHCMD_count,
} PathCmd;

#define PATH_MAX_NODES 128 * 4
typedef struct path {
    Vec2    nodes[PATH_MAX_NODES];
    size_t  node_cnt;

    Vec2    vertices[PATH_MAX_NODES*2];
    size_t  vertex_cnt;

    PathCmd commands[PATH_MAX_NODES];
} Path;

Path path_init() {
    Path path = { 0 };
    return path;
}

Path g_path;
unsigned int VBO;
unsigned int shader_program;

void path_addNode(Path *path, Vec2 node) {
    assert(path->node_cnt < PATH_MAX_NODES);
    path->nodes[path->node_cnt++] = node;
}

void path_addVertex(Path *path, Vec2 vertex) {
    assert(path->vertex_cnt < PATH_MAX_NODES*2);
    path->vertices[path->vertex_cnt++] = vertex;
}

Vec2* path_getNode(Path *path, int index) {
    int pos = index < 0 ? path->node_cnt + index : index;
    if (pos < (int)path->node_cnt) {
        return &path->nodes[pos];
    }
    return NULL;
}

double findMiterPiece(Vec2 r0, Vec2 r1, unsigned int stroke_width) {
    float psi = PI_2 - acos(vec2_dot(r0, r1) / (vec2_len(r0) * vec2_len(r1)))/2;
    float a = stroke_width/2 / tan(psi);

    return a;
}

void handleStroke(Path *path, Vec2 p0, Vec2 p1, Vec2 r, double b, unsigned int stroke_width) {
    Vec2 n = {
        .x = -r.y,
        .y = r.x,
    };
    Vec2 n_scaled = vec2_scalarMult(vec2_norm(n), stroke_width/2);

    Vec2 rn = vec2_norm(r);

    Vec2 v;
    // Not needed, since we are using triangle_strip. The first two vertices for
    // this stroke section are already placed.
    //{
    //    Vec2 rna = vec2_scalarMult(rn, a);

    //    v = vec2_add(p0, n_scaled);
    //    v = vec2_add(v, rna);
    //    path_addVertex(path, v);

    //    v = vec2_sub(p0, n_scaled);
    //    v = vec2_sub(v, rna);
    //    path_addVertex(path, v);
    //}
    {
        Vec2 rnb = vec2_scalarMult(rn, b);

        v = vec2_add(p1, n_scaled);
        v = vec2_sub(v, rnb);
        path_addVertex(path, v);

        v = vec2_sub(p1, n_scaled);
        v = vec2_add(v, rnb);
        path_addVertex(path, v);
    }
}

void path_stroke(Path *path, unsigned int stroke_width) {
    assert(path->node_cnt >= 3);

    // Draw the first stroke
    {
        Vec2 p0 = path->nodes[0];
        Vec2 p1 = path->nodes[1];
        Vec2 p2 = path->nodes[2];

        Vec2 r0 = vec2_sub(p1, p0);
        Vec2 r1 = vec2_sub(p2, p1);

        double b = findMiterPiece(r0, r1, stroke_width);

        // Place the first two vertices manually
        Vec2 n = {
            .x = -r0.y,
            .y = r0.x,
        };
        Vec2 n_scaled = vec2_scalarMult(vec2_norm(n), stroke_width/2);
        path_addVertex(path, vec2_add(p0, n_scaled));
        path_addVertex(path, vec2_sub(p0, n_scaled));

        handleStroke(path, p0, p1, r0, b, stroke_width);
    }

    for (size_t i = 1; i < path->node_cnt-2; i++) {
        Vec2 p0 = path->nodes[i];
        Vec2 p1 = path->nodes[i+1];
        Vec2 p2 = path->nodes[i+2];

        Vec2 r0 = vec2_sub(p1, p0);
        Vec2 r1 = vec2_sub(p2, p1);

        double b = findMiterPiece(r0, r1, stroke_width);
        handleStroke(path, p0, p1, r0, b, stroke_width);
    }

    // Draw the last stroke
    {
        Vec2 p0 = path->nodes[path->node_cnt-2];
        Vec2 p1 = path->nodes[path->node_cnt-1];

        Vec2 r0 = vec2_sub(p1, p0);
        handleStroke(path, p0, p1, r0, 0, stroke_width);
    }
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
            printf("Placing new node! %f %f\n", xpos, ypos);

            path_addNode(&g_path, curr);

            prev_node = path_getNode(&g_path, -1);

            g_path.vertex_cnt = 0; // Hack.
            path_stroke(&g_path, 10.0);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, (g_path.vertex_cnt) * sizeof(Vec2), g_path.vertices, GL_DYNAMIC_DRAW);
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

    glUniform2f(glGetUniformLocation(shader_program, "view_size"), width, height);
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

    // Setup vertex array object
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Setup vertex buffer
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    Vec2 test[] = {
        {200.0, 300.0},
        {400.0, 200.0},
        {600.0, 300.0},
        {450.0, 500.0},
    };

    path_addNode(&g_path, test[0]);
    path_addNode(&g_path, test[1]);
    path_addNode(&g_path, test[2]);
    path_addNode(&g_path, test[3]);

    path_stroke(&g_path, 20.0);

    printf("path vetices: %ld\n", g_path.vertex_cnt);
    for (size_t i = 0; i < g_path.vertex_cnt; i++) {
        Vec2 v = g_path.vertices[i];
        printf("(%f, %f)\n", v.x, v.y);
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_path.vertices), g_path.vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_DOUBLE, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    Shader shaders[] = {
        { GL_VERTEX_SHADER, true, "glsl/scale.vs" },
        //{ GL_TESS_CONTROL_SHADER, true, "glsl/bezier.tcs" },
        //{ GL_TESS_EVALUATION_SHADER, true, "glsl/bezier.tes" },
        //{ GL_GEOMETRY_SHADER, true, "glsl/stroke.gs" },
        { GL_FRAGMENT_SHADER, true, "glsl/simple.fs" },
        { GL_NONE },
    };

    shader_program = gl_createProgram(shaders);
    if (!shader_program) {
        return -1;
    }
    glUseProgram(shader_program);

    glUniform1f(glGetUniformLocation(shader_program, "strokeWidth2"), 0.01);


    while (!glfwWindowShouldClose(window)) {
        glfwSwapBuffers(window);
        glfwWaitEvents();

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shader_program);
        glBindVertexArray(VAO);

        // TODO: Adjacency is currently hacked in by setting the last+1 element
        // equal to the last. This will buffer overflow.
        //glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, 4);
        //glDrawArrays(GL_LINE_STRIP_ADJACENCY, 0, g_path.node_cnt+1);

        glPointSize(3.0f);
        glDrawArrays(GL_POINTS, 0, g_path.vertex_cnt);
        glLineWidth(1.0f);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, g_path.vertex_cnt);
        //glDrawArrays(GL_TRIANGLES, 0, g_path.node_cnt);

        //glEnable(GL_LINE_SMOOTH);
        //glLineWidth(16.0f);
        //glPatchParameteri(GL_PATCH_VERTICES, 4);
        //glDrawArrays(GL_PATCHES, 0, 4);

        //glEnable(GL_LINE_SMOOTH);
        //glLineWidth(5.0f);
        //glDrawArrays(GL_LINE_STRIP, 0, g_path.node_cnt);
    }

    glfwTerminate();
    return 0;
}
