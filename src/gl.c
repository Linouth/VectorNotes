#include <glad/glad.h>

#include <stdlib.h>
#include <stdio.h>

#include "gl.h"

GLuint gl_createProgram(Shader shaders[]) {
    // Inspired by https://github.com/fcaruso/GLSLParametricCurve
    // TODO: Add some debugging
    int success;

    GLuint program = glCreateProgram();

    Shader *shader = shaders;
    while (shader->type != GL_NONE) {
        shader->id = glCreateShader(shader->type);

        if (shader->is_path) {
            FILE *fp = fopen(shader->source, "r");

            fseek(fp, 0, SEEK_END);
            size_t len = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            char * const buf = malloc(len + 1);

            fread(buf, 1, len, fp);
            fclose(fp);

            buf[len] = '\0';

            glShaderSource(shader->id, 1, (const char *const *)&buf, NULL);
            free(buf);
        } else {
            glShaderSource(shader->id, 1, &shader->source, NULL);
        }

        glCompileShader(shader->id);

        glGetShaderiv(shader->id, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info[512];
            glGetShaderInfoLog(shader->id, 512, NULL, info);
            printf("Error(GL): Shader compilation failed;\n%s\n", info);
            return 0;
        }

        glAttachShader(program, shader->id);
        shader++;
    }

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, 512, NULL, info);
        printf("Error(GL): Shader program linking failed;\n%s\n", info);
        return 0;
    }

    shader = shaders;
    while (shader->type != GL_NONE) {
        glDeleteShader(shader->id);
        shader->id = 0;
        shader++;
    }

    return program;
}
