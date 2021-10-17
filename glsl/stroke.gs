#version 450 core

//layout(triangles) in;
layout(lines_adjacency) in;
//layout(lines) in;
layout(triangle_strip, max_vertices = 16) out;

uniform float strokeWidth2;

const float PI = 3.1415926535897932384626433832795;
const float PI_2 = 1.57079632679489661923;

const mat2 T = mat2(0, -1, 1, 0);

void HandleStroke(vec2 p0, vec2 p1, float a, float b) {
    vec2 r = p1 - p0;
    vec2 n = T * r;
    vec2 nScaled = strokeWidth2 * normalize(n);

    vec2 rn = normalize(r);

    gl_Position = vec4(p0 + nScaled + rn*a, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(p0 - nScaled - rn*a, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(p1 + nScaled - rn*b, 0.0, 1.0);
    EmitVertex();

    gl_Position = vec4(p1 - nScaled + rn*b, 0.0, 1.0);
    EmitVertex();

    EndPrimitive();
}

float FindMiterPiece(vec2 r0, vec2 r1) {
    float psi = PI_2 - acos(dot(r0, r1) / (length(r0) * length(r1)))/2;
    float a = strokeWidth2 / tan(psi);

    return a;
}

void main() {
    vec2 p0 = gl_in[0].gl_Position.xy;
    vec2 p1 = gl_in[1].gl_Position.xy;
    vec2 p2 = gl_in[2].gl_Position.xy;
    vec2 p3 = gl_in[3].gl_Position.xy;

    vec2 r0 = p1 - p0;
    vec2 r1 = p2 - p1;
    vec2 r2 = p3 - p2;

    // Miter join
    float a = 0;
    if (p1 != p2) {
        a = FindMiterPiece(r0, r1);
    }

    float b = 0;
    if (p2 != p3) {
        b = FindMiterPiece(r1, r2);
    }

    HandleStroke(p1, p2, a, b);


    //for (int i = 0; i < 3; i++) {
    //    gl_Position = gl_in[i].gl_Position;
    //    EmitVertex();
    //}

    //EndPrimitive();
}
