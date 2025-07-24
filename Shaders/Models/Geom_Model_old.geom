#version 430 core

layout(std430, binding = 0) buffer Positions{
    vec4 pos[];                                 // buffer for visual debug
};

layout(std430, binding = 1) buffer NumDebug{
    vec4 dbug[];                                // buffer for numerical debug
};

in vec2 vs_out_tex[];
in vec3 vs_out_norm[];
in vec3 vs_out_merged[];    // merged normals

out vec3 gs_out_tex;
out vec3 gs_out_norm;
out vec3 gs_out_merged;
out vec3 gs_out_pos;
out mat4 gs_out_M;              // transformation from scene space to texture space
out vec3 gs_out_Meye;           // cam position in texture space
out mat4 gs_out_T;              // transformation from scene space to unit prism space
out vec3 gs_out_Teye;           // cam position in unit prism space
out mat3x2 gs_out_triangle;     // base triangle verteces in texture space

uniform mat4 world;
uniform mat4 viewProj;
uniform float modelNormalMult = .5;
uniform vec3 camPos;

uniform sampler2D coneMap;
uniform vec2 HMres;
uniform int maxSteps = 50;

uniform int rayMarchingTechnique = 0;
uniform int showSteps = 0;
uniform int showEnterExit = 0;

layout(triangles) in;  
layout(triangle_strip, max_vertices = 18) out;

//
//  Find intersections for debug
//
int SSBOPadding = 5;
int pcount = 2;
vec2 getHC_texture(vec2 uv) {
    return texture(coneMap, uv);    // .r is the height; .g is the tangent of the cone
}
void findIntersection_linearSearch(vec3 u1, vec3 u2) {
    vec3 v = u2 - u1;
    int stepCount = 0;
    for (float t = 0.; t <= 1.1 && stepCount <= maxSteps; t += 1./64.) {
        vec3 u = u1 + t * v;

        pos[SSBOPadding + (pcount++)] = vec4(u, 1);

        vec2 txt = getHC_texture(u.xy);
        if (txt.r > u.z) {
            // hit found
            return;
        }
        ++stepCount;
    }
}
float getNextStep(vec3 u, vec3 v, vec3 e, out vec2 t) {       // current intersection point, view vector (camera -> u1)
    vec2 tex = getHC_texture(u.xy);       // texture at the initial point

    // vec2 tex = vec2(0, 1.);

    t = tex;
    vec3 a = vec3(u.xy, tex.r);           // vertex of the cone

    float ctga = 1. / tex.g;

    float sq = ((e.y - a.y) / (e.x - a.x)) * ((e.y - a.y) / (e.x - a.x));
    float gamma = sqrt(1. + sq);

    float t1 = (a.z - u.z) / (v.z - gamma * v.x * ctga);
    float t2 = (a.z - u.z) / (v.z + gamma * v.x * ctga);

    return max(t1, t2);
}
void findIntersection_coneStepMapping_new(vec3 u1, vec3 u2, vec3 e) {   // u1: enter point, u2: exit point in texture space

    int flags = 0;

    // pre-check
    if (getHC_texture(u1.xy).r >= u1.z) {
        pos[SSBOPadding + (pcount++)] = vec4(u1, 1.);

        flags = int(false) |
            (int(false) << 1) |
            (int(true)  << 2);
        dbug[16] = vec4(flags, 0, 0, 0);
    }

    vec3 v = u2 - u1;            // direction vector from u1 to u2

    dbug[15] = vec4(v, 0);

    float maxT = 1.;      // t parameter of the exit point (u2)

    vec3 ui = u1 + 0.000001 * v;
    float aiz = 0;
    float t = 0.000001;                     // t parameter of the intersection point (u1 -> ui)
    float ti = t + 1.;                      // t parameter of the current step (ui -> ui+1)

    int stepCount = 0;
    
    while(
        stepCount <= maxSteps &&        // max step count reached => divergent
        t < maxT &&       	    // Stay within prism
        ti > 0.0001 * t                 // Stop if cone is close to surface
        // && ui.z > aiz
    ) {
        vec2 tex;
        ti = getNextStep(ui, v, e, tex);
        t += ti;
        ui = u1 + t * v;
        aiz = getHC_texture(ui.xy).r;
        pos[SSBOPadding + (pcount++)] = vec4(ui, 1);
        ++stepCount;

        dbug[16 + (stepCount - 1) * 2 + 0] = vec4(ti, t, tex);
        dbug[16 + (stepCount - 1) * 2 + 1] = vec4(ui, 0);

    }

    flags = int(stepCount > maxSteps) |
            (int(t >= maxT) << 1) |
            (int(ti <= 0.0001 * t)  << 2);

    dbug[0] = vec4(stepCount, flags, 0, 0);

    return;
}

// unit prism intersection
bool intersectUnitPrism(vec3 p0, vec3 v, out float tNear, out float tFar) {     // p0, v in unit prism space
    float n = -1e10, f = 1e10; // near, far

    vec3 t0 = -p0 / v; // a solution for each cardinal normal
    if (v.x > 0.) { n = max(n, t0.x); } else { f = min(f, t0.x); }
    if (v.y > 0.) { n = max(n, t0.y); } else { f = min(f, t0.y); }
    if (v.z > 0.) { n = max(n, t0.z); } else { f = min(f, t0.z); }

    vec3 q = vec3(1, 1, 0) - p0;
    float t1 = q.y / v.y;
    if (v.y < 0.) { n = max(n, t1); } else { f = min(f, t1); }
    float t2 = (q.x + q.z) / (v.x + v.z);
    if (v.x + v.z < 0.) { n = max(n, t2); } else { f = min(f, t2); }

    tNear = n;
    tFar = f;
    return n < f;
}

void main() {

    // p - scene space positions
    vec4 verteces[] = {
        world * gl_in[0].gl_Position,                                                     // a - 0
        world * gl_in[1].gl_Position,                                                     // b - 1
        world * gl_in[2].gl_Position,                                                     // c - 2
        world * (gl_in[0].gl_Position + vec4(vs_out_merged[0] * modelNormalMult, 0.0)),   // d - 3
        world * (gl_in[1].gl_Position + vec4(vs_out_merged[1] * modelNormalMult, 0.0)),   // e - 4
        world * (gl_in[2].gl_Position + vec4(vs_out_merged[2] * modelNormalMult, 0.0))    // f - 5
    };

    // u - texture space positions
    vec4 texPos[] = {
        vec4(vs_out_tex[0], 0, 1),
        vec4(vs_out_tex[1], 0, 1),
        vec4(vs_out_tex[2], 0, 1),
        vec4(vs_out_tex[0], 1, 1),
        vec4(vs_out_tex[1], 1, 1),
        vec4(vs_out_tex[2], 1, 1)
    };

    // v - unit prism space positions
    vec4 uprismPos[] = {
        vec4(0, 0, 0, 1),
        vec4(1, 0, 0, 1),
        vec4(0, 0, 1, 1),
        vec4(0, 1, 0, 1)
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
    mat4 v = {
        uprismPos[0],
        uprismPos[1],
        uprismPos[2],
        uprismPos[3]
    };

    mat4 invP = inverse(p);
    mat4 M = u * invP;          // transformation from scene to texture
    mat4 T = v * invP;          // transformation from scene to unit prism

    mat4 invM = inverse(M);
    mat4 invT = inverse(T);

    // setup pipeline variables
    gs_out_M = M;
    gs_out_Meye = (M * vec4(camPos, 1)).xyz;
    gs_out_T = T;
    gs_out_Teye = (T * vec4(camPos, 1)).xyz;

    // set numerical debug values
    dbug[3] = M[0];
    dbug[4] = M[1];
    dbug[5] = M[2];
    dbug[6] = M[3];
    dbug[7] = M * dbug[1];

    dbug[8] = T[0];
    dbug[9] = T[1];
    dbug[10] = T[2];
    dbug[11] = T[3];
    dbug[12] = T * dbug[1];

    // save texture to world transformation to the SSBO
    pos[1] = invM[0];
    pos[2] = invM[1];
    pos[3] = invM[2];
    pos[4] = invM[3];

    // find entry and exit points
    vec4 p0 = T * pos[SSBOPadding];
    vec4 p1 = T * pos[SSBOPadding + 1];
    vec3 direction = normalize((p1 - p0).xyz);
    float tNear = 0;
    float tFar = 0;
    if (intersectUnitPrism(p0.xyz, direction, tNear, tFar)) {

        vec4 pNear = M * invT * (p0 +  vec4(direction * tNear, 0));
        vec4 pFar = M * invT * (p0 + vec4(direction * tFar, 0));

        dbug[13] = pNear;
        dbug[14] = pFar;

        if (showEnterExit > 0) {
            pos[SSBOPadding + (pcount++)] = pNear;
        }
        if (showSteps > 0) {
            if (rayMarchingTechnique == 0) {
                findIntersection_linearSearch(pNear.xyz, pFar.xyz);
            } else if (rayMarchingTechnique == 1) {
                findIntersection_coneStepMapping_new(pNear.xyz, pFar.xyz, (M * pos[SSBOPadding]).xyz);
            }
        }
        if (showEnterExit > 0) {
            pos[SSBOPadding + (pcount++)] = pFar;
        }
    }

    // draw unit prism
    /*
    for (int i = 0; i < 6; ++i) {
        pos[SSBOPadding + (pcount++)] = T * verteces[i]; // uprismPos[i];
    }
    */

    pos[0] = vec4(pcount, 0, 0, 0);

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
    gs_out_tex = texPos[0].xyz;
    gs_out_norm = -vs_out_norm[0];
    gs_out_merged = -vs_out_merged[0];
    gs_out_pos = verteces[0].xyz;
    gl_Position = viewProj * verteces[0];
    EmitVertex();

    gs_out_tex = texPos[1].xyz;
    gs_out_norm = -vs_out_norm[1];
    gs_out_merged = -vs_out_merged[1];
    gs_out_pos = verteces[1].xyz;
    gl_Position = viewProj * verteces[1];
    EmitVertex();
    gs_out_tex = texPos[2].xyz;
    gs_out_norm = -vs_out_norm[2];
    gs_out_merged = -vs_out_merged[2];
    gs_out_pos = verteces[2].xyz;
    gl_Position = viewProj * verteces[2];
    EmitVertex();
    EndPrimitive();

    // Top
    gs_out_tex = texPos[3].xyz;
    gs_out_norm = vs_out_norm[0];
    gs_out_merged = -vs_out_merged[0];
    gs_out_pos = verteces[3].xyz;
    gl_Position = viewProj * verteces[3];
    EmitVertex();
    gs_out_tex = texPos[4].xyz;
    gs_out_norm = vs_out_norm[1];
    gs_out_merged = -vs_out_merged[1];
    gs_out_pos = verteces[4].xyz;
    gl_Position = viewProj * verteces[4];
    EmitVertex();
    gs_out_tex = texPos[5].xyz;
    gs_out_norm = vs_out_norm[2];
    gs_out_merged = -vs_out_merged[2];
    gs_out_pos = verteces[5].xyz;
    gl_Position = viewProj * verteces[5];
    EmitVertex();
    EndPrimitive();

    // Side 1
    vec3 norm1 = normalize(cross(verteces[0].xyz - verteces[1].xyz, verteces[0].xyz - verteces[3].xyz));
    gs_out_norm = norm1;
    gs_out_merged = norm1;

    gs_out_tex = texPos[0].xyz;
    gs_out_pos = verteces[0].xyz;
    gl_Position = viewProj * verteces[0];
    EmitVertex();
    gs_out_tex = texPos[1].xyz;
    gs_out_pos = verteces[1].xyz;
    gl_Position = viewProj * verteces[1];
    EmitVertex();
    gs_out_tex = texPos[3].xyz;
    gs_out_pos = verteces[3].xyz;
    gl_Position = viewProj * verteces[3];
    EmitVertex();
    gs_out_tex = texPos[4].xyz;
    gs_out_pos = verteces[4].xyz;
    gl_Position = viewProj * verteces[4];
    EmitVertex();
    EndPrimitive();

    // Side 2
    vec3 norm2 = normalize(cross(verteces[1].xyz - verteces[4].xyz, verteces[2].xyz - verteces[1].xyz));
    gs_out_norm = norm2;
    gs_out_merged = norm2;

    gs_out_tex = texPos[1].xyz;
    gs_out_pos = verteces[1].xyz;
    gl_Position = viewProj * verteces[1];
    EmitVertex();
    gs_out_tex = texPos[2].xyz;
    gs_out_pos = verteces[2].xyz;
    gl_Position = viewProj * verteces[2];
    EmitVertex();
    gs_out_tex = texPos[4].xyz;
    gs_out_pos = verteces[4].xyz;
    gl_Position = viewProj * verteces[4];
    EmitVertex();
    gs_out_tex = texPos[5].xyz;
    gs_out_pos = verteces[5].xyz;
    gl_Position = viewProj * verteces[5];
    EmitVertex();
    EndPrimitive();

    // Side 3
    vec3 norm3 = normalize(cross(verteces[0].xyz - verteces[2].xyz, verteces[5].xyz - verteces[2].xyz));
    gs_out_norm = norm3;
    gs_out_merged = norm3;

    gs_out_tex = texPos[2].xyz;
    gs_out_pos = verteces[2].xyz;
    gl_Position = viewProj * verteces[2];
    EmitVertex();
    gs_out_tex = texPos[0].xyz;
    gs_out_pos = verteces[0].xyz;
    gl_Position = viewProj * verteces[0];
    EmitVertex();
    gs_out_tex = texPos[5].xyz;
    gs_out_pos = verteces[5].xyz;
    gl_Position = viewProj * verteces[5];
    EmitVertex();
    gs_out_tex = texPos[3].xyz;
    gs_out_pos = verteces[3].xyz;
    gl_Position = viewProj * verteces[3];
    EmitVertex();
    EndPrimitive();
}