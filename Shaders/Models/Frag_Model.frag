#version 430

//
// VARIABLES IN THE PIPELINE
//
in vec4 gs_out_tex;
in vec3 gs_out_norm;
in vec4 gs_out_pos;
in mat4 gs_out_M;
in vec4 gs_out_Meye;
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
uniform int maxSteps = 100;
uniform vec3 camPos;
uniform bool discardFragments = true;
uniform vec3 lightDir = vec3(0,-1.,0);
uniform float lightIntensity = 1.;
uniform bool displayNonConverged = false;
uniform float epsilon = 0.0;

uniform int refine_steps = 1;

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
mat2x3 mul(mat2x2 A, mat2x3 B) {
    return mat2x3(
        vec3(A[0][0] * B[0][0] + A[0][1] * B[1][0], A[0][0] * B[0][1] + A[0][1] * B[1][1], A[0][0] * B[0][2] + A[0][1] * B[1][2]),
        vec3(A[1][0] * B[0][0] + A[1][1] * B[1][0], A[1][0] * B[0][1] + A[1][1] * B[1][1], A[1][0] * B[0][2] + A[1][1] * B[1][2])
    );
}
vec2 getHC_texture(vec2 uv)
{
    return texture(coneMap, uv);    // .r is the height; .g is the tangent of the cone
}
float getH(vec2 uv) {
    return getHC_texture(uv).r;
}

vec3 getNormalTBN_finiteDiff(vec2 uv)
{
    const float mutliplier = 1.;
    vec2 delta = HMres_r * mutliplier;
    vec2 one_over_delta = HMres / mutliplier;
    
    vec2 du = vec2(delta.x, 0);
    vec2 dv = vec2(0, delta.y);
    float dhdu = 0.5 * one_over_delta.x * (getH(uv + du) - getH(uv - du));
    float dhdv = 0.5 * one_over_delta.y * (getH(uv + dv) - getH(uv - dv));
    return normalize(vec3(-dhdu, -dhdv, 1));
}
vec3 getNormalTBN(vec2 uv)
{
    return getNormalTBN_finiteDiff(uv);
}

//
// REFINE FUNCTIONS
//
vec2 refineIntersection_linearApprox(HMapIntersection interval, vec2 u0, vec2 u1)
{
    float t0 = interval.last_t;
    float t1 = interval.t;
    float h0 = getH(mix(u0, u1, t0));
    float h1 = getH(mix(u0, u1, t1));
    float dt = t1 - t0;
    float t = (dt + t0 * h1 - t1 * h0) / (dt + h1 - h0);
    t = clamp(t, t0, t1);
    return mix(u0, u1, t);
}

vec2 refineIntersection_binarySearch(HMapIntersection interval, vec2 u0, vec2 u1)
{
    float t0 = interval.last_t;
    float t1 = interval.t;
    float th = 0.5 * (t0 + t1);
    for (uint i = 0; i < refine_steps; ++i)
    {
        float fh = getH(mix(u0, u1, th));
        if (fh > 1 - th)
            t1 = th;
        else
            t0 = th;
        th = 0.5 * (t0 + t1);
    }
    
    return mix(u0, u1, th);
}

//
// INTERSECTING FUNCTIONS
//
HMapIntersection findIntersection_bumpMapping(vec2 u, vec2 u2)
{
    HMapIntersection ret = INIT_INTERSECTION;
    ret.uv = u2;
    ret.t = 1;
    ret.last_t = 1;
    return ret;
}

HMapIntersection findIntersection_parallaxMapping(vec2 u, vec2 u2)
{
    float t = 1 - getH(u2);
    HMapIntersection ret = INIT_INTERSECTION;
    ret.uv = (1 - t) * u + t * u2;
    ret.t = t;
    ret.last_t = t;
    return ret;
}

HMapIntersection findIntersection_linearSearch(vec3 u1, vec3 u2)
{
    HMapIntersection ret = INIT_INTERSECTION;
    vec3 v = normalize(u2 - u1);
    for (float t = 0.; t <= 1.0; t += .01) {
        vec3 u = u1 + t * v;

        if (u.x > 1. || u.x < 0. || u.y > 1. || u.y < 0.) {
            ret.t = t;
            return ret;
        }

        vec2 txt = getHC_texture(u.xy);
        if (txt.r > u.z) {
            // hit found
            return HMapIntersection(
                u.xy, t, t, true
            );
        } 
    }

    return ret;
}

HMapIntersection findIntersection_coneStepMapping(vec2 u, vec2 u2)
{
    vec3 ds = vec3(u2 - u, 1);
    ds = normalize(ds);
    float w = 1. / HMres.x;
    float iz = sqrt(1.0 - ds.z * ds.z); // = length(ds.xy)
    float sc = 0;
    vec2 t = getHC_texture(u);
    int stepCount = 0;
    float zTimesSc = 0.0;
    while (1.0 - ds.z * sc > t.x && stepCount < maxSteps)
    {
        zTimesSc = ds.z * sc;
        sc += relax * (w + (1.0 - zTimesSc - t.x) / (ds.z + iz / (t.y)));
        t = getHC_texture(u + ds.xy * sc);
        ++stepCount;
    }
    
    HMapIntersection ret = INIT_INTERSECTION;
    ret.last_t = zTimesSc;
    ret.wasHit = (stepCount < maxSteps);
    sc -= w;
    float tt = ds.z * sc;
    ret.uv = (1 - tt) * u + tt * u2;
    ret.t = tt;
    return ret;
}

HMapIntersection findIntersection_coneStepMapping_new(vec3 u1, vec3 u2) {
    vec3 v = normalize(u2 - u1);            // direction vector from u1 to u2
    float tgb = length(v.xy) / (-v.z);    // tangent between v and -normal

    // initial data
    vec3 ui = u1;
    vec2 tex = getHC_texture(ui.xy);        // texture at the initial point
    vec3 ai = vec3(ui.xy, tex.x);           // vertex of the cone
    float t = 0.;                           // t parameter of the intersection point
    vec2 dif = vec2(10.);

    int stepCount = 0;
    do {
        // evaluate current point
        if (length(dif * HMres) < 1. || ui.z < ai.z || ui.x < 0 || ui.x > 1 || ui.y < 0 || ui.y > 1) {
        // if (lenui < 1. || ui.x < 0 || ui.x > 1. || ui.y < 0 || ui.y > 1.) {
            // found intersection
            return HMapIntersection(ui.xy, t, 0, true);
        }

        // take step
        float tga = tex.y;
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
   
    return INIT_INTERSECTION;
}

float crossProd(vec2 p1, vec2 p2, vec2 p3) {
    return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
}

bool isPointInTriangle(vec2 p, vec2 v0, vec2 v1, vec2 v2) {
    float c0 = crossProd(v0, v1, p);
    float c1 = crossProd(v1, v2, p);
    float c2 = crossProd(v2, v0, p);

    return (c0 >= 0.0 && c1 >= 0.0 && c2 >= 0.0) || (c0 <= 0.0 && c1 <= 0.0 && c2 <= 0.0);
}

//
// MAIN FUNCTION
//
void main() {

    /*
    fs_out_col = gs_out_tex;
    return;
    */

    vec3 col = vec3(.5);

    vec4 u1 = gs_out_tex; // original texcoords
    vec4 p = gs_out_pos;  // fragment world pos

    // camera position in tangent space
    vec4 Meye = gs_out_Meye;

    // direction of ray in tangent space
    vec4 v = normalize(u1 - Meye);
    v.w = 1.;

    /*
    fs_out_col = abs(v);
    return;
    */

    vec4 u2;

    float t;
    if (-epsilon < v.z && v.z < epsilon) {      // the ray is perpendicular to the planes
        fs_out_col = vec4(1,0,0,1);
        return;
    } else if (v.z > 0) {                       // the ray intersects the bottom plane first
        t = (1 - u1.z) / v.z;
    } else {                                    // the ray intersects the top plane first
        t = -u1.z / v.z;
    }

    // texcoords where the camera ray intersects the bottom/top plane
    u2 = u1 + t * v;

    
    // fs_out_col = vec4((u2 + 1.) * .5);
    // return;
    

    // find the intersection with the height map
    // HMapIntersection I = findIntersection_bumpMapping(u, u2);
    // HMapIntersection I = findIntersection_linearSearch(u1.xyz, u2.xyz);
    // HMapIntersection I = findIntersection_coneStepMapping(u1.xy, u2.xy);
    HMapIntersection I =  findIntersection_coneStepMapping_new(u1.xyz, u2.xyz);

    // vec2 u3 = refineIntersection_linearApprox(I, u, u2);
    vec2 u3 = I.uv; // no refine function applied

    
    // fs_out_col = vec4(I.t);
    // return;
    

    // intersection is outside of the object
    if (!isPointInTriangle(u3, gs_out_triangle[0], gs_out_triangle[1], gs_out_triangle[2]))
    {
        if (discardFragments) {
            discard;
        }
        fs_out_col = vec4(0, 1, 0, 1);
        return;
    }
    
    /*
    // fetch the final albedo color
    vec3 albedo = texture(texImage, u3).xyz;
    col *= albedo;
    
    vec3 norm = vec3(0,1,0);

    float diffuse = lightIntensity * clamp(dot(-normalize(lightDir), norm), 0, 1.);
    col.rgb *= diffuse;
    */

    col = texture(texImage, u3).xyz;
    // col = vec3(1,1,0);
    // col = vec3(I.t);

    if (!I.wasHit) {
        if (displayNonConverged) {
            col.rgb = vec3(1,0,1);
        } else {
            discard;
        }
    }
    
    fs_out_col = vec4(col, 1.);
}