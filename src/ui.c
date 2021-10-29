#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "gl.h"
#include "path.h"
#include "ui.h"
#include "vec.h"

UI g_ui = {0};

static void setViewport(unsigned width, unsigned height) {
    UI *ui = &g_ui;

    glViewport(0, 0, width, height);

    ui->width = width;
    ui->height = height;

    for (size_t i = 0; i < sizeof(ui->shader)/sizeof(GLuint); i++) {
        glProgramUniform2f(
                ui->shader[i],
                glGetUniformLocation(ui->shader[i], "viewSize"),
                width, height);
    }
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

//int g_mouse_states[NUM_MOUSE_STATES] = {0};
//double g_xpos, g_ypos;

static void mousePositionCallback(GLFWwindow* window, double xpos, double ypos) {
    UI *ui = &g_ui;

    ui->mouse_pos.x = xpos;
    ui->mouse_pos.y = ypos;

    /*
    static Vec2 *prev_node = NULL;
    static double prev_len = 0;

    static int prev_state = 0;

    //static double prev_time = 0;

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
            //cmp = 10;

            // Whenever the cursor moves back, place a node to capture this movement
            if (curr_len > prev_len)
                prev_len = curr_len;
        }

        //if (fn < 0.7 && vec2_dist(*prev_node, curr) > 8) {
        if (curr_len < (prev_len - 5.0) || vec2_dist(*prev_node, curr) > cmp) {
        //glfwGetTime()
        //double t = glfwGetTime();
        //if (t - prev_time > 0.01) {
            path_addNode(g_path, curr, -1);

            prev_node = path_getNode(g_path, -1);
            prev_len = 0;
            //prev_time = t;
        }
    } else {
        // Mouse not held
        prev_node = NULL;
        prev_len = 0;

        if (prev_state == GLFW_PRESS) {
            // Mouse was released

            printf("Refitting line\n");
            path_deinit(new);
            //new = path_fitBezier(g_path, 5.0, 25.0, 3);
            new = path_fitBezier(g_path);
        }
    }

    prev_state = g_mouse_states[GLFW_MOUSE_BUTTON_LEFT];
    */
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    UI *ui = &g_ui;

    ui->mouse_state[button] = action;

    //if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    //    Vec2 node = {
    //        .x = g_xpos,
    //        .y = g_ypos,
    //    };

    //    path_addNode(g_path, node, -1);
    //    printf("Added a node at %f %f\n", g_xpos, g_ypos);
    //}
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    UI *ui = &g_ui;

    setViewport(width, height);
    ui->width = width;
    ui->height = height;
}

UI *ui_init(unsigned width, unsigned height) {
    //UI *ui = malloc(sizeof(UI));
    UI *ui = &g_ui;

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

        setViewport(width, height);

        glfwSetFramebufferSizeCallback(ui->window, framebufferSizeCallback);
        glfwSetKeyCallback(ui->window, keyCallback);
        glfwSetMouseButtonCallback(ui->window, mouseButtonCallback);
        glfwSetCursorPosCallback(ui->window, mousePositionCallback);
    }

    glGenVertexArrays(VAO_count, ui->vao);
    glGenBuffers(VBO_count, ui->vbo);
    for (int i = 0; i < VAO_count; i++) {
        assert(i < VBO_count);
        glBindVertexArray(ui->vao[i]);
        glBindBuffer(GL_ARRAY_BUFFER, ui->vbo[i]);

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

        ui->shader[SHADER_simple] = gl_createProgram(shaders);
    }
    {
        Shader shaders[] = { // SHADER_spline
            { GL_VERTEX_SHADER, true, "glsl/scale.vs" },
            { GL_GEOMETRY_SHADER, true, "glsl/stipple.gs" },
            { GL_FRAGMENT_SHADER, true, "glsl/simple.fs" },
            { GL_NONE },
        };

        ui->shader[SHADER_stipple] = gl_createProgram(shaders);
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
        ui->shader[SHADER_debug] = gl_createProgram(shaders);
    }

    return ui;
}

void ui_deinit(UI *ui) {
    for (size_t i = 0; i < sizeof(ui->shader)/sizeof(GLuint); i++) {
        glDeleteProgram(ui->shader[i]);
    }
    glDeleteBuffers(sizeof(ui->vbo)/sizeof(GLuint), ui->vbo);
    glDeleteVertexArrays(sizeof(ui->vao)/sizeof(GLuint), ui->vao);

    glfwDestroyWindow(ui->window);

    //free(ui);
}

void ui_drawSpline(UI *ui, Path *path) {
    GLuint color_loc;

    glUseProgram(ui->shader[SHADER_simple]);
    glBindVertexArray(ui->vao[VAO_spline]);

    glBindBuffer(GL_ARRAY_BUFFER, ui->vbo[VBO_spline]);
    glBufferData(GL_ARRAY_BUFFER, path->node_cnt*sizeof(Vec2), path->nodes, GL_DYNAMIC_DRAW);

    glPointSize(4.0f);
    color_loc = glGetUniformLocation(ui->shader[SHADER_simple], "color");

    glUniform3f(color_loc, 0.70, 0.70, 0.70);
    glDrawArrays(GL_POINTS, 0, path->node_cnt);


    glUseProgram(ui->shader[SHADER_stipple]);

    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1.0f);
    color_loc = glGetUniformLocation(ui->shader[SHADER_stipple], "color");

    glUniform3f(color_loc, 0.173, 0.325, 0.749);
    glDrawArrays(GL_LINE_STRIP, 0, path->node_cnt);
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
