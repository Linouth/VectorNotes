#pragma once

#include <glad/glad.h>
#include <stdbool.h>

typedef struct shader {
    GLenum      type;
    bool        is_path;
    const char* source;
    GLuint      id;
} Shader;

GLuint gl_createProgram(Shader shaders[]);
