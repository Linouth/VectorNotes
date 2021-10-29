#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <assert.h>

#include "vec.h"
#include "gl.h"
#include "path.h"
#include "ui.h"

UI *ui_init() {
    UI *ui = malloc(sizeof(UI));

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

    free(ui);
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
