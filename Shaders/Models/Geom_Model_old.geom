#version 430 core

in vec2 vs_out_tex[];
in vec3 vs_out_norm[];
in vec3 vs_out_merged[];    // merged normals

out vec4 gs_out_tex;
out vec3 gs_out_norm;
out vec4 gs_out_pos;
out mat4 gs_out_M;
out vec4 gs_out_Meye;
out mat3x2 gs_out_triangle;

uniform mat4 world;
uniform mat4 viewProj;
uniform float modelNormalMult = .5;
uniform vec3 camPos;

layout(triangles) in;  
layout(triangle_strip, max_vertices = 18) out;

void main()
{
    // p
    vec4 verteces[] = {
        world * gl_in[0].gl_Position,                                                     // a - 0
        world * gl_in[1].gl_Position,                                                     // b - 1
        world * gl_in[2].gl_Position,                                                     // c - 2
        world * (gl_in[0].gl_Position + vec4(vs_out_merged[0] * modelNormalMult, 0.0)),   // d - 3
        world * (gl_in[1].gl_Position + vec4(vs_out_merged[1] * modelNormalMult, 0.0)),   // e - 4
        world * (gl_in[2].gl_Position + vec4(vs_out_merged[2] * modelNormalMult, 0.0))    // f - 5
    };

    // u
    vec4 texPos[] = {
        vec4(vs_out_tex[0], 0, 1),
        vec4(vs_out_tex[1], 0, 1),
        vec4(vs_out_tex[2], 0, 1),
        vec4(vs_out_tex[0], 1, 1),
        vec4(vs_out_tex[1], 1, 1),
        vec4(vs_out_tex[2], 1, 1)
    };

    gs_out_triangle = mat3x2(
        texPos[0].xy,
        texPos[1].xy,
        texPos[2].xy
    );

    // M = [u0 u1 u2 u3] * [p0 p1 p2 p3]^-1
    mat4 u = {
        texPos[0],
        texPos[1],
        texPos[2],
        texPos[3]
    };
    mat4 p = {
        verteces[0],
        verteces[1],
        verteces[2],
        verteces[3]
    };
    mat4 M = u * inverse(p);

    gs_out_M = M;
    gs_out_Meye = M * vec4(camPos, 1);

    //
    // TRIANGLE STRIP
    //

    /*
    // e d b a c d f e c b
    // 4 3 1 0 2 3 5 4 2 1
    int indeces[] = {4, 3, 1, 0, 2, 3, 5, 4, 2, 1};

    for (int i = 0; i < 10; ++i) {
        gs_out_tex = texPos[indeces[i]];
        gs_out_norm = vs_out_norm[int(mod(indeces[i], 3))];
        gs_out_pos = verteces[indeces[i]];
        gl_Position = verteces[indeces[i]];
        EmitVertex();
    }
    EndPrimitive();
    */

    //
    // TRIALNGLES
    //

    // Bottom
    gs_out_tex = texPos[0];
    gs_out_norm = -vs_out_norm[0];
    gs_out_pos = verteces[0];
    gl_Position = viewProj * verteces[0];
    EmitVertex();

    gs_out_tex = texPos[1];
    gs_out_norm = -vs_out_norm[1];
    gs_out_pos = verteces[1];
    gl_Position = viewProj * verteces[1];
    EmitVertex();
    gs_out_tex = texPos[2];
    gs_out_norm = -vs_out_norm[2];
    gs_out_pos = verteces[2];
    gl_Position = viewProj * verteces[2];
    EmitVertex();
    EndPrimitive();

    // Top
    gs_out_tex = texPos[3];
    gs_out_norm = vs_out_norm[0];
    gs_out_pos = verteces[3];
    gl_Position = viewProj * verteces[3];
    EmitVertex();
    gs_out_tex = texPos[4];
    gs_out_norm = vs_out_norm[1];
    gs_out_pos = verteces[4];
    gl_Position = viewProj * verteces[4];
    EmitVertex();
    gs_out_tex = texPos[5];
    gs_out_norm = vs_out_norm[2];
    gs_out_pos = verteces[5];
    gl_Position = viewProj * verteces[5];
    EmitVertex();
    EndPrimitive();

    // Side 1
    vec3 norm1 = normalize(cross(verteces[0].xyz - verteces[1].xyz, verteces[0].xyz - verteces[3].xyz));
    gs_out_tex = texPos[0];
    gs_out_norm = norm1;
    gs_out_pos = verteces[0];
    gl_Position = viewProj * verteces[0];
    EmitVertex();
    gs_out_tex = texPos[1];
    gs_out_norm = norm1;
    gs_out_pos = verteces[1];
    gl_Position = viewProj * verteces[1];
    EmitVertex();
    gs_out_tex = texPos[3];
    gs_out_norm = norm1;
    gs_out_pos = verteces[3];
    gl_Position = viewProj * verteces[3];
    EmitVertex();
    gs_out_tex = texPos[4];
    gs_out_norm = norm1;
    gs_out_pos = verteces[4];
    gl_Position = viewProj * verteces[4];
    EmitVertex();
    EndPrimitive();

    // Side 2
    vec3 norm2 = normalize(cross(verteces[1].xyz - verteces[4].xyz, verteces[2].xyz - verteces[1].xyz));
    gs_out_tex = texPos[1];
    gs_out_norm = norm2;
    gs_out_pos = verteces[1];
    gl_Position = viewProj * verteces[1];
    EmitVertex();
    gs_out_tex = texPos[2];
    gs_out_norm = norm2;
    gs_out_pos = verteces[2];
    gl_Position = viewProj * verteces[2];
    EmitVertex();
    gs_out_tex = texPos[4];
    gs_out_norm = norm2;
    gs_out_pos = verteces[4];
    gl_Position = viewProj * verteces[4];
    EmitVertex();
    gs_out_tex = texPos[5];
    gs_out_norm = norm2;
    gs_out_pos = verteces[5];
    gl_Position = viewProj * verteces[5];
    EmitVertex();
    EndPrimitive();

    // Side 3
    vec3 norm3 = normalize(cross(verteces[0].xyz - verteces[2].xyz, verteces[5].xyz - verteces[2].xyz));
    gs_out_tex = texPos[2];
    gs_out_norm = norm3;
    gs_out_pos = verteces[2];
    gl_Position = viewProj * verteces[2];
    EmitVertex();
    gs_out_tex = texPos[0];
    gs_out_norm = norm3;
    gs_out_pos = verteces[0];
    gl_Position = viewProj * verteces[0];
    EmitVertex();
    gs_out_tex = texPos[5];
    gs_out_norm = norm3;
    gs_out_pos = verteces[5];
    gl_Position = viewProj * verteces[5];
    EmitVertex();
    gs_out_tex = texPos[3];
    gs_out_norm = norm3;
    gs_out_pos = verteces[3];
    gl_Position = viewProj * verteces[3];
    EmitVertex();
    EndPrimitive();
}