// debug buffers
layout(std430, binding = 0) buffer VisDebug{
    vec4 vdbug[];                                 // buffer for visual debug
};

layout(std430, binding = 1) buffer NumDebug{
    vec4 ndbug[];                                // buffer for numerical debug
};

// uniforms
uniform sampler2D coneMap;
uniform int maxSteps = 50;

// structs
struct UnitIntersection{
    bool found;
    float near;
    float far;
};
struct Ray{
    vec3 start;
    vec3 dir;
};
struct IntersectParams{
    vec3 enter;
    vec3 exit;
    vec3 cam;
    int vDebugStart;
};
struct IntersectReturn{
    int flags;
    int stepCount;
};
struct StepParams {
    vec3 point;
    Ray view;
};
struct StepReturn{
    float t;
    vec2 conemapData;
};

// debug functions
void visualDebugSet(int index, vec4 data) {
    vdbug[index] = data;
}
void visualDebugSet(int index, mat4x4 data) {
    for (int i = 0; i < 4; ++i) {
        vdbug[index + i] = data[i];
    }
}
vec4 visualDebugGet(int index) {
    return vdbug[index];
}
void numericalDebugSet(int index, vec4 data) {
    ndbug[index] = data;
}
void numericalDebugSet(int index, mat4x4 data) {
    for (int i = 0; i < 4; ++i) {
        ndbug[index + i] = data[i];
    }
}
vec4 numericalDebugGet(int index) {
    return ndbug[index];
}

// conemap texture handling
vec2 conemap_get(vec2 uv) {
    return texture(coneMap, uv);    // .r is the height; .g is the tangent of the cone
}
float conemap_getHeight(vec2 uv) {
    return conemap_get(uv).r;
}

StepReturn getNextStep(StepParams params) {       // current intersection point, view vector (u1 -> u2)

    vec3 u = params.point;
    vec3 e = params.view.start;
    vec3 v = params.view.dir;

    StepReturn val;

    vec2 tex = conemap_get(u.xy);       // texture at the initial point
    val.conemapData = tex;

    vec3 a = vec3(u.xy, tex.r);           // vertex of the cone
    float ctga = 1.f / tex.g;
    float sq = ((e.y - a.y) / (e.x - a.x)) * ((e.y - a.y) / (e.x - a.x));
    float gamma = sqrt(1.f + sq);
    float t1 = (a.z - u.z) / (v.z - gamma * v.x * ctga);
    float t2 = (a.z - u.z) / (v.z + gamma * v.x * ctga);

    val.t = max(t1, t2);
    return val;
}
IntersectReturn findIntersection_coneStepMapping(IntersectParams params) {   // u1: enter point, u2: exit point in texture space
    vec3 u1 = params.enter;
    vec3 u2 = params.exit;
    int stepCount = 0;
    int flags = 0;

    // pre-check
    if (conemap_getHeight(u1.xy) >= u1.z) {

        visualDebugSet(params.vDebugStart + stepCount, vec4(u1, 1.));

        flags = int(false) |
            (int(false) << 1) |
            (int(true)  << 2);

        numericalDebugSet(0, vec4(stepCount, flags, 0, 0));
        return IntersectReturn(flags, 0);
    }

    vec3 v = u2 - u1;            // direction vector from u1 to u2

    numericalDebugSet(15, vec4(v, 0));

    float maxT = 1.;      // t parameter of the exit point (u2)

    vec3 ui = u1 + 0.000001f * v;
    float aiz = 0.f;
    float t = 0.000001f;                     // t parameter of the intersection point (u1 -> ui)
    float ti = t + 1.f;                      // t parameter of the current step (ui -> ui+1)

    while(
        stepCount <= maxSteps &&        // max step count reached => divergent
        t < maxT &&       	    // Stay within prism
        ti > 0.0001f * t                 // Stop if cone is close to surface
        // && ui.z > aiz
    ) {
        vec2 tex;
        StepReturn stepData = getNextStep(StepParams(ui, Ray(u1, v)));
        ti = stepData.t;
        t += ti;
        ui = u1 + t * v;
        aiz = conemap_getHeight(ui.xy);

        visualDebugSet(params.vDebugStart + stepCount, vec4(ui, 1));
        numericalDebugSet(16 + stepCount * 2, vec4(ti, t, stepData.conemapData));
        numericalDebugSet(16 + stepCount * 2 + 1, vec4(ui, 0));

        ++stepCount;
    }

    flags = int(stepCount > maxSteps) |
            (int(t >= maxT) << 1) |
            (int(ti <= 0.0001 * t)  << 2);

    numericalDebugSet(0, vec4(stepCount, flags, 0, 0));

    return IntersectReturn(flags, stepCount);
}

// unit prism intersection
UnitIntersection intersectUnitPrism(Ray ray) {     // ray in unit prism space
    float n = -1e10, f = 1e10; // near, far

    vec3 p0 = ray.start;
    vec3 v = ray.dir;

    vec3 t0 = -p0 / v; // a solution for each cardinal normal
    if (v.x > 0.) { n = max(n, t0.x); } else { f = min(f, t0.x); }
    if (v.y > 0.) { n = max(n, t0.y); } else { f = min(f, t0.y); }
    if (v.z > 0.) { n = max(n, t0.z); } else { f = min(f, t0.z); }

    vec3 q = vec3(1, 1, 0) - p0;
    float t1 = q.y / v.y;
    if (v.y < 0.) { n = max(n, t1); } else { f = min(f, t1); }
    float t2 = (q.x + q.z) / (v.x + v.z);
    if (v.x + v.z < 0.) { n = max(n, t2); } else { f = min(f, t2); }

    return UnitIntersection(n < f, n, f);
}