#version 430 core

layout(std430, binding = 0) buffer Positions{
    vec4 pos[];
};
uniform sampler2D coneMap;
uniform vec2 HMres;
uniform int maxSteps = 50;

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

layout(triangles) in;  
layout(triangle_strip, max_vertices = 18) out;


//
//  Find input and output points
//
struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Triangle {
    vec3 v0, v1, v2;
};

bool rayIntersectsTriangle(Ray ray, Triangle tri, out float t) {
    vec3 edge1 = tri.v1 - tri.v0;
    vec3 edge2 = tri.v2 - tri.v0;
    vec3 h = cross(ray.direction, edge2);
    float a = dot(edge1, h);
    
    if (abs(a) < 1e-6) return false;  // Ray is parallel to the triangle

    float f = 1.0 / a;
    vec3 s = ray.origin - tri.v0;
    float u = f * dot(s, h);
    
    if (u < 0.0 || u > 1.0) return false;

    vec3 q = cross(s, edge1);
    float v = f * dot(ray.direction, q);
    
    if (v < 0.0 || u + v > 1.0) return false;

    t = f * dot(edge2, q);
    
    return t >= 0.0;  // Intersection must be in the positive direction
}

bool rayIntersectsPrism(Ray ray, Triangle faces[8], out vec3 entry, out vec3 exit) {
    float tMin = 1e10, tMax = -1e10;
    bool hasEntry = false, hasExit = false;

    for (int i = 0; i < 8; i++) {
        float t;
        if (rayIntersectsTriangle(ray, faces[i], t)) {
            vec3 intersection = ray.origin + t * ray.direction;
            if (t < tMin) {
                tMin = t;
                entry = intersection;
                hasEntry = true;
            }
            if (t > tMax) {
                tMax = t;
                exit = intersection;
                hasExit = true;
            }
        }
    }

    return hasEntry && hasExit;
}

int pcount = 2;
vec2 getHC_texture(vec2 uv)
{
    return texture(coneMap, uv);    // .r is the height; .g is the tangent of the cone
}
void findIntersection_linearSearch(vec3 u1, vec3 u2, mat4 invM)
{
    vec3 v = u2 - u1;

    int stepCount;
    for (float t = 0.; t <= 1.1; t += 1./64.) {
        vec3 u = u1 + t * v;

        pos[pcount] = invM * vec4(u, 1);
        ++pcount;

        if (u.x > 1. || u.x < 0. || u.y > 1. || u.y < 0.) {
            // out of triangle
            return;
        }

        vec2 txt = getHC_texture(u.xy);
        if (txt.r > u.z) {
            // hit found
            return;
        } 
    }
}
void findIntersection_coneStepMapping_new(vec3 u1, vec3 u2, mat4 invM) {
    vec3 v = normalize(u2 - u1);            // direction vector from u1 to u2
    float tgb = length(v.xy) / (-v.z);      // tangent between v and -normal

    // initial data
    vec3 ui = u1 + 0.001 * v;
    vec2 tex = getHC_texture(ui.xy);        // texture at the initial point
    vec3 ai = vec3(ui.xy, tex.x);           // vertex of the cone
    float t = 0.;                           // t parameter of the intersection point
    vec2 dif = vec2(10.);

    int stepCount = 0;
    do {
        
        pos[pcount] = invM * vec4(ui, 1);
        ++pcount;

        // evaluate current point
        if (length(dif * HMres) < 0.1 || dot(ui-u1,v)<0) {
            // found intersection or ui is out of the cube
            return;
        }

        // take step
        float tga = tex.y;
        if (tgb + tga == 0) {                           // ray is parallel to cone
            return;
        }

        float hi = (tgb * (ui.z - ai.z)) / (tgb + tga);
        float xi = ai.z + hi;
        float ti = (xi - ui.z) / v.z;

        t = (xi - u1.z) / v.z;

        // setup new intersection point
        vec3 nui = ui + ti * v;
        dif = abs(nui.xy - ui.xy);
        ui = nui;
        tex = getHC_texture(ui.xy);
        ai = vec3(ui.xy, tex.x);

        ++stepCount;
    } while(stepCount <= maxSteps);
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

void main()
{
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
        vec4(0, 1, 0, 1),
        vec4(1, 1, 0, 1),
        vec4(0, 1, 1, 1)
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

    // find entry and exit points
    vec4 p0 = T * pos[1];
    vec4 p1 = T * pos[2];
    vec3 direction = normalize((p1 - p0).xyz);
    float tNear = 0;
    float tFar = 0;
    if (intersectUnitPrism(p0.xyz, direction, tNear, tFar)) {
        ++pcount;
        pos[3] = invT * (p0 +  vec4(direction * tNear, 0));   
        ++pcount;
        pos[4] = invT * (p0 + vec4(direction * tFar, 0));
    }

    // draw unit prism
    /*
    for (int i = 0; i < 6; ++i) {
        pos[++pcount] = T * verteces[i]; // uprismPos[i];
    }
    */

    // find intersections for debug
    /*
    vec3 entry;
    vec3 exit;
    Triangle faces[8];
    faces[0] = Triangle(verteces[0].xyz, verteces[2].xyz, verteces[1].xyz);
    faces[1] = Triangle(verteces[3].xyz, verteces[4].xyz, verteces[5].xyz);
    faces[2] = Triangle(verteces[0].xyz, verteces[1].xyz, verteces[3].xyz);
    faces[3] = Triangle(verteces[1].xyz, verteces[4].xyz, verteces[3].xyz);
    faces[4] = Triangle(verteces[1].xyz, verteces[2].xyz, verteces[4].xyz);
    faces[5] = Triangle(verteces[2].xyz, verteces[5].xyz, verteces[4].xyz);
    faces[6] = Triangle(verteces[2].xyz, verteces[0].xyz, verteces[5].xyz);
    faces[7] = Triangle(verteces[0].xyz, verteces[3].xyz, verteces[2].xyz);
    if(rayIntersectsPrism(Ray(pos[1].xyz, normalize(pos[2].xyz - pos[1].xyz)), faces, entry, exit)) {
        ++pcount;
        // findIntersection_linearSearch((M * vec4(entry, 1)).xyz, (M * vec4(exit, 1)).xyz, invM);
        // findIntersection_coneStepMapping_new((M * vec4(entry, 1)).xyz, (M * vec4(exit, 1)).xyz, invM);
        pos[4] = vec4(entry, 1);
        // pos[4] = vec4(exit, 1);
    }
    */

    pos[0] = vec4(pcount, 0, 0, 0);

    gs_out_M = M;
    gs_out_Meye = (M * vec4(camPos, 1)).xyz;
    gs_out_T = T;
    gs_out_Teye = (T * vec4(camPos, 1)).xyz;

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