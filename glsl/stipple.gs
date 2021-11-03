#version 450 core

layout(lines) in;
layout(line_strip, max_vertices = 32) out;

const double PIECE_LEN = 0.04;
const int SPLIT = 6;

void main()
{
    //vec2 v0 = gl_in[0].gl_Position.xy;
    //vec2 v1 = gl_in[1].gl_Position.xy;
    //vec2 r = v1 - v0;
    //vec2 t = normalize(r);
    //double len = length(r);

    //gl_Position = vec4(v0 + 0*PIECE_LEN*t, 0, 1);
    //EmitVertex();
    //gl_Position = vec4(v0 + (0+1)*PIECE_LEN*t, 0, 1);
    //EmitVertex();
    //EndPrimitive();

    //gl_Position = vec4(v0 + 2*PIECE_LEN*t, 0, 1);
    //EmitVertex();
    //gl_Position = vec4(v0 + (2+1)*PIECE_LEN*t, 0, 1);
    //EmitVertex();
    //EndPrimitive();

    //gl_Position = vec4(v0 + 4*PIECE_LEN*t, 0, 1);
    //EmitVertex();
    //gl_Position = vec4(v0 + (4+1)*PIECE_LEN*t, 0, 1);
    //EmitVertex();
    //EndPrimitive();

    //gl_Position = gl_in[0].gl_Position;
    //EmitVertex()
    //gl_Position = gl_in[1].gl_Position;
    //EmitVertex()
    //EndPrimitive();

    for (int i = 0; i < SPLIT; i++) {
        gl_Position = vec4(v0 + (2*i)*len/(SPLIT*2)*t, 0, 1);
        EmitVertex();
        gl_Position = vec4(v0 + (2*i+1)*len/(SPLIT*2)*t, 0, 1);
        EmitVertex();
        EndPrimitive();
    }
}
