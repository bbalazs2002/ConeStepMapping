#version 430

//
// VARIABLES IN THE PIPELINE
//
in vec3 gs_out_tex;
in vec3 gs_out_norm;
in vec3 gs_out_merged;
in vec3 gs_out_pos;
in mat4 gs_out_M;
in vec3 gs_out_Meye;
in mat4 gs_out_T;
in vec3 gs_out_Teye;
in mat3x2 gs_out_triangle;

out vec4 fs_out_col;

//
// UNIFORMS
//

// textures
uniform sampler2D texImage;
uniform sampler2D coneMap;

// uniforms
uniform vec2 HMres;     // height map resolution (w, h)
uniform vec2 HMres_r;   // reciprical of the height map resolution (1/w, 1/h)
uniform float relax = 1.;
uniform int maxSteps = 50;
uniform vec3 camPos;
uniform bool discardFragments = true;
uniform vec3 lightDir = vec3(0,-1.,0);
uniform float lightIntensity = 1.;
uniform bool displayNonConverged = false;
uniform float epsilon = 0.0;

uniform int refine_steps = 1;

uniform int rayMarchingTechnique = 0;
uniform int showFlags = 0;

//
// INTERSECTION DATA
//
struct HMapIntersection{
    vec2 uv;
    float t;
    float last_t;
    bool wasHit;
};
const HMapIntersection INIT_INTERSECTION = { 0.0.xx, 0.0, 0.0, false };

//
// HELPER FUNCTIONS
//
vec2 getHC_texture(vec2 uv) {
    return texture(coneMap, uv);    // .r is the height; .g is the tangent of the cone
}
float getH(vec2 uv) {
    return getHC_texture(uv).r;
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

float getNextStep(vec3 u, vec3 v) {       // current intersection point, view vector (camera -> u1)
    vec2 tex = getHC_texture(u.xy);       // texture at the initial point
    vec3 a = vec3(u.xy, tex.r);           // vertex of the cone
    vec3 e = gs_out_Meye;

    float ctga = 1. / tex.g;
    float gamma = sqrt(1 + pow((e.y - a.y) / (e.x - a.x), 2));

    float t1 = (a.z - u.z) / (v.z - gamma * v.x * ctga);
    float t2 = (a.z - u.z) / (v.z + gamma * v.x * ctga);

    return max(t1, t2);
}

HMapIntersection findIntersection_coneStepMapping_new(vec3 u1, vec3 u2, out int flags) {   // u1: enter point, u2: exit point in texture space

    // pre-check
    if (getH(u1.xy) >= u1.z) {
        flags = int(false) |
            (int(false) << 1) |
            (int(true)  << 2);
        return HMapIntersection( vec2(u1.xy), 0, 0, true );
    }

    vec3 v = u2 - u1;            // direction vector from u1 to u2

    float maxT = 1.;      // t parameter of the exit point (u2)

    vec3 ui = u1 + 0.0001 * v;
    float t = 0.0001;  // t parameter of the intersection point (u1 -> ui)
    float ti = t + 1.;                     // t parameter of the current step (ui -> ui+1)

    int stepCount = 0;
    
    while(
        stepCount <= maxSteps &&        // max step count reached => divergent
        t < maxT &&       	    // Stay within prism
        ti > 0.0001 * t                 // Stop if cone is close to surface
    ) {
        ti = getNextStep(ui, v);
        t += ti;
        ui = u1 + t * v;
        ++stepCount;
    }

    flags = int(stepCount > maxSteps) |
            (int(t >= maxT) << 1) |
            (int(ti <= 0.0001 * t)  << 2);

    /*
    if (t >= maxT + .001) {
        discard;
    }
    */

    HMapIntersection val = INIT_INTERSECTION;

    val.wasHit = bool(flags & 4) && !bool(flags ^ 4);

    val.t = t;
    val.uv = ui.xy;

    return val;
}

//
// MAIN FUNCTION
//
void main() {

    // fs_out_col = vec4(gs_out_norm * .5 + .5, 1);
    // fs_out_col = vec4(1,0,0,1);
    // fs_out_col = vec4(abs(gs_out_norm), 1);
    // return;

    vec3 col = vec3(.5);

    vec3 u1 = gs_out_tex;           // original texcoords
    vec3 p = gs_out_pos;            // fragment world pos
    mat4 T = gs_out_T;
    mat4 M = gs_out_M;

    // vec3 Meye = gs_out_Meye;        // camera position in tangent space
    // vec3 v = normalize(u1 - Meye);  // direction of ray in tangent space

    vec3 p0 = gs_out_Teye;
    vec3 p1 = (T * vec4(p, 1)).xyz;
    vec3 v = normalize(p1 - p0);

    float tNear = 0;
    float tFar = 0;
    if (!intersectUnitPrism(gs_out_Teye, v, tNear, tFar)) {     // cannot happen
        if (discardFragments) {
            discard;
        }
        fs_out_col = vec4(0, 1, 1, 1);
        return;
    }

    vec3 uu = gs_out_Teye + tFar * v;               // exit point in unit prism space
    vec3 u2 = (M * inverse(T) * vec4(uu, 1)).xyz;   // exit point in texture space
    
    // find the intersection with the height map
    HMapIntersection I = INIT_INTERSECTION;
    int flags;
    I = findIntersection_coneStepMapping_new(u1, u2, flags);

    if (showFlags > 0) {

        vec3 flagCol = vec3(0, 0, 0);
        if (bool(flags & 1)) {
            flagCol.r = .5;
        }
        if (bool(flags & 2)) {
            flagCol.g = .5;
        }
        if (bool(flags & 4)) {
            flagCol.b = .5;
        }

        if (I.wasHit) {
            flagCol *= 2.;
        }

        fs_out_col = vec4(flagCol, 1);
        return;
    }
        
    // fs_out_col = vec4(0, 0, I.wasHit, 1);
    // return;

    if (!I.wasHit) {
        discard;
    }

    // fs_out_col = vec4(int(I.wasHit));
    // fs_out_col = vec4(I.uv, 0, 1.);
    // fs_out_col = vec4(I.t, 0, 0, 1.);
    // return;

    // vec2 u3 = refineIntersection_linearApprox(I, u, u2);
    vec2 u3 = I.uv; // no refine function applied
    
    /*
    // fetch the final albedo color
    vec3 albedo = texture(texImage, u3).xyz;
    col *= albedo;
    
    vec3 norm = vec3(0,1,0);

    float diffuse = lightIntensity * clamp(dot(-normalize(lightDir), norm), 0, 1.);
    col.rgb *= diffuse;
    */
    col = texture(coneMap, u3).xyz;

    // return;
    if (!I.wasHit) {
        if (displayNonConverged) {
            col = vec3(1,0,1);
        } else if (discardFragments) {
            discard;
        } else {
            col = vec3(0, 1, 0);
        }
    }
    
    fs_out_col = vec4(col, 1.);
}