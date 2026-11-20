// Replacement GLSL for the game's data/shapes 3D mode (data/shaders/3dmode.vs / 3dmode.fs).
// Keeps every original uniform/varying so the game's own uniform setup still works,
// and adds iso* uniforms driven by the mod. v2: cartoon look.
#pragma once

static const char* kIsoVS = R"GLSL(
uniform vec2 worldSize;
uniform float alpha;
uniform vec2 worldPosition;
uniform vec2 isoLightNudge;   // an object standing in a wall tile takes its light from the tile it faces

// --- PA3aiD additions ---
uniform int   isoEnabled;    // 1 when the iso camera is active
uniform int   isoWallMode;   // 0 = walls up, 1 = cutaway, 2 = walls down
uniform int   isoCutStyle;   // 0 = shrink, 1 = dither fade
uniform float isoCutScreen;  // > -1.5: cutaway by screen position - pieces below this clip-space y are cut (0 = lower half, 0.5 = lower 75 %)
uniform vec2  isoFocus;      // world XY of cutaway focus
uniform vec2  isoCamDir;     // normalised ground direction focus -> camera
uniform float isoCutRadius;  // lateral radius of cutaway (<=0 means unlimited)
uniform float isoStub;       // fraction of wall height kept when cut
uniform int   isoWallPass;   // 1 while the game's Render3DWalls is drawing
uniform float isoDoorSplit;  // > 0: a double door open this far - each leaf (each side of x = 0) slides away from the middle
uniform float isoWallLift;   // model-space Y offset applied to wall pieces
uniform float isoWallFat;    // wall thickness factor on the game's +-0.25 shaft (2.0 = full tile)
uniform vec2  isoWallThick;  // 1 on the axes (x,z) along which this wall piece is thin (set per piece)
uniform vec4  isoWallExt;    // 1.9.106: 1 = this piece's end towards -x, +x, -z, +z meets a door: run it to the tile edge (no gap beside the door)
uniform float isoWallBase;   // lowest model-space Y of the wall piece being drawn (set per piece)
uniform int   isoBatch;      // 1.9.130: 1 while the recorded wall batch is drawn
uniform sampler2D isoPieces; // its per-piece matrices and values
uniform vec2  isoPiecesSize;
attribute float isoPiece;    // which piece a batch vertex belongs to
uniform mat4  isoViewInv;    // inverse of the camera view matrix (eye -> world)
uniform int   isoRoomCut;    // 1 = Sims-style: cut walls that have indoor space behind them
uniform sampler2D isoOutdoors; // room map: r = indoors, a = inside a room
uniform sampler2D isoCellA;
uniform sampler2D isoCellB;
uniform sampler2D isoTiles;
uniform float isoWallArt;
varying float isoFenceV;     // 1 = this wall tile is a fence (its 2D art is mostly see-through)

varying vec3 N;
varying vec3 v;
varying vec2 currentPosition;
varying float isoIsWall;
varying float isoLipV;
varying vec2  isoThickV;
varying float isoCut;      // 0..1, how much this vertex is cut
varying vec3 isoWorld;     // world-space position (walls)
varying vec3 isoWN;        // world-space normal (walls)
varying vec3 isoModelPos;  // model-space position (the prisoner head overlay)

void main(void)
{
    vec4 vtx = gl_Vertex;
    isoModelPos = gl_Vertex.xyz;
    // 1.9.130 wall batch: the per-piece matrix and values come from isoPieces (8 texels a piece, 128 pieces a row) instead of the
    // modelview and the per-piece uniforms; the modelview is then the camera alone
    mat4 MV = gl_ModelViewMatrix;
    mat4 MVP = gl_ModelViewProjectionMatrix;
    mat3 NM = gl_NormalMatrix;
    float wBase = isoWallBase, wLift = isoWallLift;
    vec2 wThick = isoWallThick;
    vec4 wExt = isoWallExt;
    if (isoBatch == 1) {
        float row = floor((isoPiece + 0.5) / 128.0);
        float col = isoPiece - row * 128.0;
        vec2 b = vec2(col * 8.0 + 0.5, row + 0.5) / isoPiecesSize, dx = vec2(1.0 / isoPiecesSize.x, 0.0);
        vec4 r0 = texture2DLod(isoPieces, b, 0.0), r1 = texture2DLod(isoPieces, b + dx, 0.0), r2 = texture2DLod(isoPieces, b + 2.0 * dx, 0.0);
        vec4 pa = texture2DLod(isoPieces, b + 3.0 * dx, 0.0);
        wExt = texture2DLod(isoPieces, b + 4.0 * dx, 0.0);
        mat4 Mw = mat4(vec4(r0.x, r1.x, r2.x, 0.0), vec4(r0.y, r1.y, r2.y, 0.0), vec4(r0.z, r1.z, r2.z, 0.0), vec4(r0.w, r1.w, r2.w, 1.0));
        MV = gl_ModelViewMatrix * Mw;
        MVP = gl_ProjectionMatrix * MV;
        vec3 a0 = MV[0].xyz, a1 = MV[1].xyz, a2 = MV[2].xyz;
        vec3 c0 = cross(a1, a2), c1 = cross(a2, a0), c2 = cross(a0, a1);
        NM = mat3(c0, c1, c2) * (dot(a0, c0) < 0.0 ? -1.0 : 1.0);      // inverse transpose, up to scale (normals are normalised)
        wBase = pa.x; wLift = pa.y; wThick = pa.zw;
    }
    if (isoDoorSplit > 0.0) vtx.x += (vtx.x < 0.0 ? -1.0 : 1.0) * isoDoorSplit;      // double doors part instead of sliding as one
    bool isWall = (isoWallPass == 1);   // objects that merely sit at world (0,0) (windows, wall fittings) are not walls
    isoIsWall = isWall ? 1.0 : 0.0;
    isoThickV = wThick;
    isoLipV = (isWall && gl_Vertex.y < -0.98) ? 1.0 : 0.0;   // the game's thin full-width base lip: removed in the fragment shader
    isoCut = 0.0;
    // true world position: undo the camera part of the modelview
    vec4 wp = isoViewInv * (MV * vtx);
    isoWorld = wp.xyz;
    isoWN = (isoViewInv * vec4(normalize(NM * gl_Normal), 0.0)).xyz;

    if (isWall && isoEnabled == 1 && isoWallMode > 0)
    {
        float cut = 1.0;
        if (isoWallMode == 1 && isoCutScreen > -1.5)
        {
            // screen split (user 8 Nov): walls whose piece stands in the near part of the screen (below isoCutScreen in clip space,
            // -1 = bottom edge, +1 = top) drop to the stub; the far part stays at full height. Whole piece, by its base origin.
            vec4 cp = MVP * vec4(0.0, wBase, 0.0, 1.0);
            cut = (cp.w > 0.0 && cp.y / cp.w < isoCutScreen) ? 1.0 : 0.0;
        }
        else if (isoWallMode == 1 && isoRoomCut == 1)
        {
            // look one tile behind the wall (away from the camera); indoor -> cut
            // Drop a wall when there is indoor space behind it and the camera side is
            // not inside a room (corridor / yard). Back walls seen from inside a room stay up.
            // decide once per wall piece (its tile centre), so a piece never half-drops into a ramp
            vec2 pc = (isoViewInv * (MV * vec4(0.0, 0.0, 0.0, 1.0))).xy;
            vec2 tc = floor(pc) + 0.5;
            // sample the axis tiles as well as the diagonal so short rooms / corner pieces still cut
            vec2 sx = vec2(abs(isoCamDir.x) > 0.3 ? sign(isoCamDir.x) : 0.0, 0.0);
            vec2 sy = vec2(0.0, abs(isoCamDir.y) > 0.3 ? sign(isoCamDir.y) : 0.0);
            vec2 sd = sx + sy;
            float behindIn = max(texture2D(isoOutdoors, (tc - sd) / worldSize).r,
                             max(texture2D(isoOutdoors, (tc - sx) / worldSize).r,
                                 texture2D(isoOutdoors, (tc - sy) / worldSize).r));
            float frontRoom = min(texture2D(isoOutdoors, (tc + sd) / worldSize).a,
                              min(texture2D(isoOutdoors, (tc + sx) / worldSize).a,
                                  texture2D(isoOutdoors, (tc + sy) / worldSize).a));
            cut = (behindIn > 0.5 && frontRoom < 0.5) ? 1.0 : 0.0;
            // Sims-style: only walls near the focus and on the camera side of it drop; the rest of the building stays up
            float dFocus = length(tc - isoFocus);
            if (isoCutRadius > 0.0) cut *= 1.0 - smoothstep(isoCutRadius * 0.6, isoCutRadius, dFocus);

        }
        else if (isoWallMode == 1)
        {
            vec2 rel = wp.xy - isoFocus;
            float along = dot(rel, isoCamDir);
            float lateral = length(rel - along * isoCamDir);
            cut = step(0.0, along);
            if (isoCutRadius > 0.0) cut *= step(lateral, isoCutRadius);
        }
        isoCut = cut;
        // wall models are Y-up in model space with the floor at y = 0
        if (isoCutStyle == 0)
            vtx.y = (vtx.y - wBase) * mix(1.0, isoStub, cut) + wBase;   // wall models span y -0.989..+0.989: shrink towards the base, not the middle
    }
    // lower only the base: the base meets the floor, the top stays at the game's height (a plain shift made walls too low)
    if (isWall && isoEnabled == 1) vtx.y += wLift * clamp((0.989 - vtx.y) / 1.978, 0.0, 1.0);
    // the game builds walls half a tile thick (+-0.25) with a thin full-width lip at the base, so wall-mounted objects sit
    // a quarter tile away. Fill the whole tile: +-0.25 -> +-0.5, +-0.23 -> +-0.46 (piece end faces at +-0.5 stay put).
    isoFenceV = 0.0;
    if (isWall && isoEnabled == 1 && isoWallArt > 0.5) {
        vec2 fc = floor((isoViewInv * (MV * vec4(0.0, 0.0, 0.0, 1.0))).xy);
        vec4 fcb = texture2DLod(isoCellB, (fc + 0.5) / worldSize, 0.0);
        if (fcb.b > 0.5) {
            vec4 fca = texture2DLod(isoCellA, (fc + 0.5) / worldSize, 0.0);
            // fences are recognised by WHERE their art sits in the game's tileset (the fence sets), not by how it looks
            if (fca.x < 0.07 && fca.y > 0.695 && fca.y < 0.765) isoFenceV = 1.0;
        }
    }
    if (isWall && isoEnabled == 1) {
        float fat = isoFenceV > 0.5 ? 0.2 : isoWallFat;      // a fence is a thin screen, not a thick wall
        // an end face at the shaft (+-0.25) that meets a door goes out to the tile edge before the thickness scaling (which leaves +-0.5 alone)
        if (wExt.x > 0.5 && vtx.x < -0.15 && vtx.x > -0.4) vtx.x = -0.5;
        if (wExt.y > 0.5 && vtx.x > 0.15 && vtx.x < 0.4) vtx.x = 0.5;
        if (wExt.z > 0.5 && vtx.z < -0.15 && vtx.z > -0.4) vtx.z = -0.5;
        if (wExt.w > 0.5 && vtx.z > 0.15 && vtx.z < 0.4) vtx.z = 0.5;
        // piece ends overlap their neighbours a hair (no dark seams) - 1.9.120 (user: z-fighting bands where walls meet): 1.2 % overlapped coplanar
        // faces far enough to flicker; 0.1 % closes the seam without a visible overlap
        // 1.9.126 (user: seams at joins again): 0.1 % was a tenth of a pixel close up, so hairline cracks showed (and the outline pass drew them
        // as dashed lines). 0.4 %: the faces and tops are now painted from the world position only, so the overlap looks the same from either piece
        if (abs(vtx.x) < 0.4) vtx.x *= fat; else vtx.x *= 1.004;
        if (abs(vtx.z) < 0.4) vtx.z *= fat; else vtx.z *= 1.004;

    }

    v = vec3(MV * vtx);
    N = normalize(NM * gl_Normal);
    gl_Position = MVP * vtx;
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_FrontColor = vec4( gl_Color.rgb, gl_Color.a * alpha );

    currentPosition = vec2( (worldPosition.x + isoLightNudge.x)/worldSize.x, (worldPosition.y + isoLightNudge.y)/worldSize.y );
    // Walls do not have a currentPosition value
    if( isWall )
        currentPosition = vec2(gl_Vertex.x/worldSize.x, gl_Vertex.y/worldSize.y );
}
)GLSL";

static const char* kIsoFS = R"GLSL(
uniform sampler2D sampler0;
uniform sampler2D lightmap;
uniform sampler2D vismap;
uniform sampler2D vismapPrevious;
uniform sampler2D observers;

uniform float vismapBlend;
uniform int blackAndWhite;
uniform int noTexture;
uniform int renderFow;
uniform float bitDepth;

// --- PA3aiD additions ---
uniform int   isoEnabled;
uniform int   isoCutStyle;
uniform vec4  isoUniformTint; // a > 0: this prisoner's security-level colour, painted onto the pale uniform
uniform vec4  isoSkinTint;    // a > 0: this prisoner's own skin colour (the game's SkinColour)
uniform vec4  isoHairTint;    // a > 0: this prisoner's hair / beard colour (from its 2D head sprite)
uniform int   isoCartoon;    // 1 = cartoon shading on
uniform float isoGrit;       // 0..1 grime on walls
uniform float isoStyle;      // 0..1 how far textures are flattened to painted colour
uniform float isoToon;       // 0..1 cel-band strength
uniform float isoSat;        // saturation multiplier
uniform float isoWallHeight; // approx wall height in world units (pattern scale)
uniform vec3  isoWallTint;   // multiplier for wall colour
uniform sampler2D isoCellA;   // per cell: the game's 2D art mapping (same as the floor)
uniform sampler2D isoCellB;
uniform sampler2D isoTiles;
uniform float isoWallArt;    // 1 = colour walls from their own 2D art
uniform float isoWallFat;    // wall thickness factor (the same uniform as the vertex shader's): the top's cavity follows it
varying float isoFenceV;
uniform int   isoRoomCut;
uniform sampler2D isoOutdoors;
uniform vec2  worldSize;

varying vec2 currentPosition;
varying vec3 N;
varying vec3 v;
varying float isoIsWall;
varying float isoLipV;
varying vec2 isoThickV;
varying float isoCut;
varying vec3 isoWorld;
varying vec3 isoWN;
varying vec3 isoModelPos;
uniform sampler2D isoHeads;   // the 2D head sprites (face + hair), 8 x 8 cells
uniform vec4  isoHeadGeo;     // this body's head: centre xyz and radius, in model units (w = 0: no overlay)
uniform float isoCellsDark;   // 0..1: cell rooms' lights off (Sleep regime)
uniform vec4  isoHeadCell;    // xy = the head sprite's cell origin (uv), z = cell size (uv), w > 0.5 = bald (back of head in skin)
uniform sampler2D isoBulbs;   // lamp pools: r = indoor bulbs, g = outdoor lamps, b = near an indoor bulb
uniform float isoNight;       // 0 day .. 1 full night
uniform float isoBulbOn;
uniform float isoOutline;     // 0..1: how dark objects' edge lines are (0 = off)
uniform vec2  isoDoorClip;    // x != 0: a door cut with the walls - discard where (model y - y) * x > 0

// The light on a spot (same as the own floor): the game's lightmap, a moonlit level at night instead of a fixed grey, and each lamp's pool.
vec3 isoNightLight(vec3 lm, vec2 p, float indoor, float dayMin)
{
    vec3 lmv = mix(vec3(dot(lm, vec3(0.333))), lm, 0.5);
    vec3 moon = vec3(0.13, 0.16, 0.26);
    vec3 L = max(lmv, mix(vec3(dayMin, dayMin, dayMin + 0.06), moon, isoNight));
    if (isoBulbOn > 0.5) {
        vec4 b = texture2D(isoBulbs, p / worldSize);
        float cov = b.b * step(0.5, indoor);
        vec3 bulb = mix(vec3(1.0), vec3(1.05, 0.98, 0.88), b.r);
        L *= mix(vec3(1.0), (0.84 + 0.24 * b.r) * bulb, cov);
        float o = (1.0 - step(0.5, indoor)) * isoNight;
        L = L * (1.0 - 0.35 * o * (1.0 - b.g)) + o * b.g * vec3(1.0, 0.85, 0.60) * 0.80;
    }
    L = min(L, vec3(1.0));      // 1.9.76: never brighter than the game's full light (user: overexposed)
    return L;
}

#define MAX_LIGHTS 3

vec4 DoLighting(void)
{
   vec4 finalColor = vec4(0.0, 0.0, 0.0, 0.0);
   for (int i=0;i<MAX_LIGHTS;i++)
   {
      vec3 L = normalize(gl_LightSource[i].position.xyz - v);
      vec3 E = normalize(-v);
      vec3 R = normalize(-reflect(L,N));
      vec4 Iamb = gl_FrontLightProduct[i].ambient;
      vec4 Idiff = gl_FrontLightProduct[i].diffuse * max(dot(N,L), 0.0);
      Idiff = clamp(Idiff, 0.0, 1.0);
      vec4 Ispec = gl_FrontLightProduct[i].specular * pow(max(dot(R,E),0.0),0.3*gl_FrontMaterial.shininess);
      Ispec = clamp(Ispec, 0.0, 1.0);
      finalColor += Iamb + Idiff + Ispec;
   }
   return finalColor;
}

float isoHash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float isoNoise(vec2 p)
{
    vec2 i = floor(p); vec2 f = fract(p);
    f = f*f*(3.0-2.0*f);
    return mix(mix(isoHash(i), isoHash(i+vec2(1.0,0.0)), f.x),
               mix(isoHash(i+vec2(0.0,1.0)), isoHash(i+vec2(1.0,1.0)), f.x), f.y);
}

// Blurred texture sample: kills photographic noise, keeps the overall colour.
vec4 isoSoftSample(vec2 uv, float px)
{
    vec2 r = fwidth(uv) * px;
    vec4 s = texture2D(sampler0, uv) * 0.36;
    s += texture2D(sampler0, uv + r * 0.8) * 0.16;
    s += texture2D(sampler0, uv - r * 0.8) * 0.16;
    s += texture2D(sampler0, uv + vec2(r.x, -r.y) * 0.8) * 0.16;
    s += texture2D(sampler0, uv + vec2(-r.x, r.y) * 0.8) * 0.16;
    return s;
}

vec3 isoSaturate(vec3 c, float s)
{
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    return clamp(mix(vec3(l), c, s), 0.0, 1.0);
}

// Warm wood panelling (inside faces): vertical planks, raised panels, chair rail, stone above.
vec3 isoPanelWall(vec3 col, float along, float h, float wh)
{
    float hn = h / wh;
    float aa = fwidth(along) * 1.5 + 0.002;
    float u = along * 2.5;                                  // plank width ~0.4 tile
    float fu = fract(u);
    float plank = floor(u);
    vec3 wood = vec3(0.60, 0.38, 0.19);
    wood *= 0.90 + 0.20 * isoHash(vec2(plank, 3.0));        // plank-to-plank tone
    wood *= 0.92 + 0.12 * isoNoise(vec2(along * 3.0, h * 22.0));  // vertical grain
    wood = mix(wood, col * vec3(1.4, 1.0, 0.7), 0.12);      // keep a hint of the source art
    float groove = 1.0 - smoothstep(0.02, 0.02 + aa * 2.5, min(fu, 1.0 - fu));
    wood *= 1.0 - 0.38 * groove;
    // raised panel inset between skirting and chair rail
    float inY = smoothstep(0.10, 0.12, hn) * (1.0 - smoothstep(0.50, 0.52, hn));
    float inX = smoothstep(0.10, 0.10 + aa * 3.0, fu) * (1.0 - smoothstep(0.90 - aa * 3.0, 0.90, fu));
    float panel = inX * inY;
    wood *= 1.0 + 0.10 * panel;
    float frame = (1.0 - panel) * inY * smoothstep(0.06, 0.10, hn) ;
    wood *= 1.0 - 0.10 * frame;
    if (hn < 0.09) wood = vec3(0.30, 0.18, 0.09) * (0.9 + 0.2 * isoHash(vec2(plank, 7.0)));   // skirting
    // chair rail
    if (hn >= 0.52 && hn < 0.60) {
        wood = vec3(0.40, 0.25, 0.12);
        if (hn > 0.575) wood *= 1.25;                       // lit top edge
    }
    if (hn >= 0.60) {                                       // painted stone above the rail
        vec3 st = vec3(0.85, 0.85, 0.83) * (0.94 + 0.10 * isoHash(vec2(floor(along * 2.0), floor(h * 5.0))));
        float ln = 1.0 - smoothstep(0.03, 0.03 + aa, min(fract(h * 5.0), 1.0 - fract(h * 5.0)));
        wood = st * (1.0 - 0.10 * ln);
    }
    wood *= 1.0 - 0.20 * (1.0 - smoothstep(0.0, 0.25, hn)); // contact shadow
    return wood;
}

// Painted cinder-block wall with dark cap, dark skirting; wood panelling on room-side faces.
vec3 isoWallColour(vec3 matCol)
{
    // exact face normal from screen-space derivatives (model normals are unreliable)
    vec3 n = cross(dFdx(isoWorld), dFdy(isoWorld));
    float nl = length(n);
    n = nl > 1e-12 ? n / nl : normalize(isoWN);
    float h = abs(isoWorld.z);
    float wh = max(isoWallHeight, 0.05);
    vec3 col = isoSaturate(matCol, isoSat) * isoWallTint;
    col = mix(col, vec3(0.92, 0.90, 0.85), 0.18);         // lift toward chalky paint
    if (isoFenceV > 0.5) return vec3(0.60, 0.62, 0.66) * (0.9 + 0.1 * n.z);      // fences: galvanised steel
    // the wall's own colour, from its 2D art (red brick, white wall, security-level colours...)
    bool art = false; vec3 artCol = vec3(0.8); vec3 artTop = vec3(0.8);
    if (isoWallArt > 0.5) {
        vec2 wc = floor(isoWorld.xy - n.xy * 0.02);
        vec4 cb = texture2D(isoCellB, (wc + 0.5) / worldSize);
        if (cb.b > 0.5) {
            vec4 ca = texture2D(isoCellA, (wc + 0.5) / worldSize);
            vec2 cuv = ca.xy + 0.5 * (ca.zw - ca.xy) + 0.5 * (cb.xy - ca.xy);
            // sample the art in five places: the sides take its most coloured part (the brick, the paint), the top its lightest (the coping)
            vec2 ex = ca.zw - ca.xy, ey = cb.xy - ca.xy;
            vec3 best = vec3(0.0); float bestS = -1.0; vec3 light = vec3(0.0); float lightL = -1.0; vec3 avg = vec3(0.0);
            for (int k = 0; k < 5; k++) {
                vec2 o = k == 0 ? vec2(0.5, 0.5) : (k == 1 ? vec2(0.22, 0.22) : (k == 2 ? vec2(0.78, 0.22) : (k == 3 ? vec2(0.22, 0.78) : vec2(0.78, 0.78))));
                vec3 sc = texture2D(isoTiles, ca.xy + o.x * ex + o.y * ey, 3.5).rgb; avg += sc * 0.2;
                float mx = max(sc.r, max(sc.g, sc.b)), mn = min(sc.r, min(sc.g, sc.b)); float sat = (mx - mn) * (0.4 + mx);
                if (sat > bestS) { bestS = sat; best = sc; }
                float lm = dot(sc, vec3(0.299, 0.587, 0.114)); if (lm > lightL) { lightL = lm; light = sc; }
            }
            artCol = bestS > 0.10 ? best : mix(avg, light, 0.75);      // plain (white, grey) walls: their lightest part, not the outlines' grey
            float l = max(dot(artCol, vec3(0.299, 0.587, 0.114)), 0.001);
            artCol = clamp(artCol * mix(1.0, 0.62 / l, 0.35), 0.0, 1.0);
            artTop = light;
            art = dot(avg, vec3(0.299, 0.587, 0.114)) > 0.06 || lightL > 0.15;      // near-black art (an empty / unmapped tile) never paints a wall black: keep the default colour
        }
    }

    if (abs(n.z) > 0.6) {                                  // top cap: dark slate-blue coping
        // brick coursing on the wall top (a running bond), warm brick colour with darker mortar
        vec2 bp = isoWorld.xy; float brow = floor(bp.y / 0.2);
        float boff = mod(brow, 2.0) * 0.25;
        float bty = fract(bp.y / 0.2), btx = fract((bp.x + boff) / 0.5);
        float mort = clamp(step(bty, 0.10) + step(btx, 0.06), 0.0, 1.0);
        float bvar = 0.90 + 0.16 * isoHash(vec2(floor((bp.x + boff) / 0.5), brow));
        vec3 brick = (art ? artTop : vec3(0.80, 0.74, 0.66)) * bvar;
        brick = mix(brick, vec3(0.55, 0.52, 0.48), mort * 0.8);
        // cavity wall: two brick leaves with a dark gap between them
        vec2 cu = abs(fract(isoWorld.xy) - 0.5);
        float across = (isoThickV.x > 0.5 && isoThickV.y < 0.5) ? cu.x : ((isoThickV.y > 0.5 && isoThickV.x < 0.5) ? cu.y : min(cu.x, cu.y));
        if (isoWallArt > 0.5) {
            // 1.9.126 (user: z-fighting and seams on wall tops at corners / joins): the cavity follows the wall cells around this one (the
            // world), not the piece being drawn, so two pieces overlapping at a join paint the same top and the overlap cannot be seen
            vec2 tc = floor(isoWorld.xy) + 0.5, dd = isoWorld.xy - tc;
            bool aE = texture2D(isoCellB, (tc + vec2(1.0, 0.0)) / worldSize).b > 0.5, aW = texture2D(isoCellB, (tc - vec2(1.0, 0.0)) / worldSize).b > 0.5;
            bool aN = texture2D(isoCellB, (tc + vec2(0.0, 1.0)) / worldSize).b > 0.5, aS = texture2D(isoCellB, (tc - vec2(0.0, 1.0)) / worldSize).b > 0.5;
            bool ax = aE || aW, ay = aN || aS;
            if (ax && !ay) across = abs(dd.y);
            else if (ay && !ax) across = abs(dd.x);
            else if (!ax && !ay) across = 1.0;      // a lone post: solid top
            else {      // a corner / T / cross: the cavity runs from the middle out along each arm that has a wall
                across = 1.0;
                if (aE) across = min(across, dd.x >= 0.0 ? abs(dd.y) : length(dd));
                if (aW) across = min(across, dd.x <= 0.0 ? abs(dd.y) : length(dd));
                if (aN) across = min(across, dd.y >= 0.0 ? abs(dd.x) : length(dd));
                if (aS) across = min(across, dd.y <= 0.0 ? abs(dd.x) : length(dd));
            }
        }
        // the gap follows the wall thickness (it was fixed at 0.155 tile: right only for 0.75-tile walls)
        float gapHalf = 0.41 * 0.25 * max(isoWallFat, 0.4);
        float cav = 1.0 - smoothstep(gapHalf - 0.012, gapHalf, across);
        vec3 gap = vec3(0.36, 0.34, 0.31) * (0.85 + 0.3 * isoNoise(isoWorld.xy * 14.0));
        return mix(brick, gap, cav * 0.8) * isoWallTint;
    }
    // along-wall coordinate depends on face orientation
    float along = abs(n.x) > abs(n.y) ? isoWorld.y : isoWorld.x;
    col = art ? artCol : mix(col, vec3(0.80, 0.80, 0.78), 0.75);          // the wall's own colour (light grey stone if unknown)
    float rowH = wh / 6.0;
    float row = floor(h / rowH);
    float bw = rowH * 2.2;
    float off = mod(row, 2.0) * 0.5 * bw;
    vec2 cell = vec2(fract((along + off) / bw), fract(h / rowH));
    float edge = min(min(cell.x, 1.0 - cell.x) * bw / rowH, min(cell.y, 1.0 - cell.y));
    float aa = fwidth(h / rowH) * 1.5 + 0.001;
    float line = 1.0 - smoothstep(0.04, 0.04 + aa, edge);
    col *= 1.0 - 0.13 * line;
    col *= 0.96 + 0.08 * isoHash(vec2(floor((along + off) / bw), row));   // per-block variation

    float hn = h / wh;
    if (hn < 0.14) col *= 0.72;                                        // skirting band
    col *= 1.0 - 0.18 * (1.0 - smoothstep(0.0, 0.35, hn));             // ambient occlusion at floor

    // light grime, cartoon-subtle
    float stain = isoNoise(vec2(along * 1.3, hn * 2.5) + isoWorld.xy * 0.21);
    col *= 1.0 - isoGrit * 0.22 * smoothstep(0.55, 0.9, stain);
    float streak = smoothstep(0.80, 0.97, isoNoise(vec2(along * 7.0, 0.0))) * smoothstep(0.3, 1.0, hn);
    col = mix(col, col * vec3(0.70, 0.55, 0.42), isoGrit * 0.45 * streak);
    return col;
}

void main(void)
{
    if (isoEnabled == 1 && isoIsWall > 0.5 && isoLipV > 0.02) discard;
    if (isoDoorClip.x != 0.0 && (isoModelPos.y - isoDoorClip.y) * isoDoorClip.x > 0.0) discard;
    bool cartoon = (isoEnabled == 1 && isoCartoon == 1);

    // Dither-fade for cut walls (screen-door transparency, no sorting needed)
    if (isoEnabled == 1 && isoIsWall > 0.5 && isoCutStyle == 1 && isoCut > 0.01)
    {
        vec2 fc = floor(mod(gl_FragCoord.xy, 4.0));
        float bayer = mod(fc.x * 3.0 + fc.y * 2.0 + fc.x * fc.y * 5.0, 16.0) / 16.0;
        float keepTop = 1.0 - 0.85 * isoCut;
        if (abs(isoWorld.z) > 0.2 * isoWallHeight && bayer > keepTop) discard;
    }

    if (isoEnabled == 1 && isoIsWall > 0.5 && isoFenceV > 0.5) {      // fence: posts, rails and a diamond mesh; the rest is open
        vec3 fn = cross(dFdx(isoWorld), dFdy(isoWorld)); fn = length(fn) > 1e-12 ? normalize(fn) : vec3(0.0, 0.0, 1.0);
        float fh = abs(isoWorld.z), fwh = max(isoWallHeight, 0.05);
        if (abs(fn.z) < 0.6) {
            float al = abs(fn.x) > abs(fn.y) ? isoWorld.y : isoWorld.x;
            float pa = fract(al + 0.5);
            bool post = pa < 0.05 || pa > 0.95;
            bool rail = fh > fwh * 0.93 || fh < fwh * 0.05 || abs(fh - fwh * 0.5) < fwh * 0.015;
            float gs = 0.2; float lw = 0.07;      // an open chain-link diamond, thin wire (no widening when zoomed out: it read as a solid wall)
            bool mesh = fract((al + fh) / gs) < lw || fract((al - fh) / gs) < lw;
            if (!(post || rail)) { float far_ = clamp(fwidth(al) * 40.0 - 0.5, 0.0, 1.0); vec2 fq = floor(mod(gl_FragCoord.xy, 4.0)); float bay = mod(fq.x * 3.0 + fq.y * 2.0 + fq.x * fq.y * 5.0, 16.0) / 16.0;
                if (!mesh || bay < far_ * 0.7) discard; }      // far away the wire thins out by dithering, so the fence stays see-through
        }
    }
    vec4 lightMapSample = texture2D( lightmap, currentPosition );
    vec4 finalColor = gl_Color * lightMapSample;
    vec4 lit = DoLighting();

    if (!cartoon)
    {
        if( noTexture == 0 ) finalColor *= texture2D( sampler0, gl_TexCoord[0].xy );
        finalColor *= lit;
    }
    else
    {
        // never let the night lightmap crush walls/models to black: keep a moonlit floor level
        // dusk / lamp light tints everything orange: keep only half of the colour cast
        // 1.9.72: lit from the world position (walls had no real lightmap position: they read a corner of the map and went navy).
        // A wall face takes the light of the tile it faces; a wall top the brightest tile beside it.
        vec2 lp = isoWorld.xy;
        vec3 lmRaw = lightMapSample.rgb;
        if (isoIsWall > 0.5) {
            vec3 fn = cross(dFdx(isoWorld), dFdy(isoWorld));
            fn = length(fn) > 1e-12 ? normalize(fn) : vec3(0.0, 0.0, 1.0);
            if (abs(fn.z) < 0.6) {
                vec2 wn = length(isoWN.xy) > 0.3 ? normalize(isoWN.xy) : normalize(fn.xy);
                lp += wn * 0.55;
                lmRaw = texture2D(lightmap, lp / worldSize).rgb;
            } else {
                vec2 c0 = floor(lp) + 0.5;
                lmRaw = max(max(texture2D(lightmap, (c0 + vec2(1.0, 0.0)) / worldSize).rgb, texture2D(lightmap, (c0 - vec2(1.0, 0.0)) / worldSize).rgb),
                            max(texture2D(lightmap, (c0 + vec2(0.0, 1.0)) / worldSize).rgb, texture2D(lightmap, (c0 - vec2(0.0, 1.0)) / worldSize).rgb));
            }
        }
        float indoorL = texture2D(isoOutdoors, (floor(lp) + 0.5) / worldSize).r;
        finalColor.rgb = gl_Color.rgb * isoNightLight(lmRaw, lp, indoorL, isoIsWall > 0.5 ? 0.52 : 0.34);
        vec4 tex = vec4(1.0);
        if (noTexture == 0)
        {
            vec4 sharp = texture2D(sampler0, gl_TexCoord[0].xy);
            tex = sharp;
            if (isoIsWall < 0.5 && isoStyle > 0.01)          // walls are procedural: skip the blur taps
            {
                vec4 soft = isoSoftSample(gl_TexCoord[0].xy, 3.0);
                tex = mix(sharp, soft, isoStyle);
                tex.a = sharp.a;
            }
        }
        vec3 albedo = (isoIsWall > 0.5) ? isoWallColour(tex.rgb) : isoSaturate(tex.rgb, isoSat);
        {   // very vivid, bright colours (prisoner orange, bright uniforms) are toned down: less saturated and darker
            float mxc = max(albedo.r, max(albedo.g, albedo.b)), mnc = min(albedo.r, min(albedo.g, albedo.b));
            float vivid = smoothstep(0.45, 0.9, (mxc - mnc) / (mxc + 1e-4)) * smoothstep(0.35, 0.8, mxc);
            float lum0 = dot(albedo, vec3(0.299, 0.587, 0.114));
            albedo = mix(albedo, vec3(lum0), 0.30 * vivid) * (1.0 - 0.22 * vivid);
        }

        if (isoUniformTint.a > 0.5) {      // the stock uniform is saturated orange (240,144,16); skin (240,192,128) and hair are not: recolour the orange only
            vec3 t0 = tex.rgb; float mxu = max(t0.r, max(t0.g, t0.b)), mnu = min(t0.r, min(t0.g, t0.b));
            float sat = (mxu - mnu) / (mxu + 1e-4), gr = t0.g / (t0.r + 1e-4), br = t0.b / (t0.r + 1e-4);
            float uni = smoothstep(0.62, 0.8, sat) * smoothstep(0.35, 0.5, t0.r) * (1.0 - smoothstep(0.66, 0.76, gr)) * (1.0 - smoothstep(0.2, 0.32, br));
            uni *= smoothstep(0.04, 0.09, distance(t0, vec3(0.482, 0.267, 0.075))) * smoothstep(0.04, 0.09, distance(t0, vec3(0.549, 0.388, 0.075)));      // never the dark skin / brown hair swatches (they read as orange too: heads came out in the uniform colour)
            vec3 tcol = mix(vec3(dot(isoUniformTint.rgb, vec3(0.333))), isoUniformTint.rgb, 0.85) * 0.82;      // the game's table colours are pure (Protected = 255,255,0): glowed in 3D
            albedo = mix(albedo, tcol * (0.2 + 0.8 * mxu), uni);
            float stripe = 1.0 - smoothstep(0.04, 0.09, distance(t0, vec3(1.0, 0.718, 0.333)));      // the pale front stripe: a darker shade of the same colour
            albedo = mix(albedo, tcol * 0.62, stripe);
        }
        if (isoSkinTint.a > 0.5 || isoHairTint.a > 0.5) {      // the stock prisoner models paint skin and hair with flat swatches: swap them for this prisoner's own (after the uniform, which also catches the dark skin swatch)
            vec3 t0 = tex.rgb;
            float sk = max(1.0 - smoothstep(0.03, 0.08, distance(t0, vec3(0.988, 0.776, 0.541))), 1.0 - smoothstep(0.03, 0.08, distance(t0, vec3(0.482, 0.267, 0.075))));
            float hr = max(1.0 - smoothstep(0.03, 0.08, distance(t0, vec3(0.302, 0.075, 0.0))), max(1.0 - smoothstep(0.03, 0.08, distance(t0, vec3(0.549, 0.388, 0.075))), 1.0 - smoothstep(0.03, 0.08, distance(t0, vec3(0.169, 0.078, 0.020)))));
            if (isoSkinTint.a > 0.5) albedo = mix(albedo, isoSkinTint.rgb, sk);
            if (isoHairTint.a > 0.5) albedo = mix(albedo, isoHairTint.rgb, hr);
        }
        if (isoHeadGeo.w != 0.0) {      // the head is an overlay, as in 2D: paint this prisoner's own head sprite onto the model's head (face on the front, hair / skin behind)
            vec3 d = isoModelPos - isoHeadGeo.xyz; float r = abs(isoHeadGeo.w), rv = abs(isoHeadCell.w), fs = isoHeadGeo.w < 0.0 ? -1.0 : 1.0;      // r across, rv up (the game scales models differently)
            float inHead = (1.0 - smoothstep(r * 1.05, r * 1.25, length(d.xz))) * smoothstep(-rv * 0.95, -rv * 0.7, d.y);
            if (inHead > 0.0) {
                vec3 dir = normalize(vec3(d.x / r, d.y / rv, fs * d.z / r));      // which way this bit of the head faces
                vec2 huv = vec2(0.5 + fs * dir.x * 0.47, 0.5 - dir.y * 0.47);      // the 2D head wraps the whole front half of the head
                vec4 hs = texture2D(isoHeads, isoHeadCell.xy + clamp(huv, 0.02, 0.98) * isoHeadCell.z);
                if (max(hs.r, max(hs.g, hs.b)) < 0.2 && length(huv - 0.5) > 0.34) hs.a = 0.0;      // the 2D outline ring: not on a 3D head
                vec3 skinC = isoSkinTint.a > 0.5 ? isoSkinTint.rgb : vec3(0.988, 0.776, 0.541);
                float hmx = max(hs.r, max(hs.g, hs.b)), hmn = min(hs.r, min(hs.g, hs.b));
                // the face is a white mask the game tints with the skin colour: every grey pixel (white skin, black features and the
                // anti-aliased greys between them) becomes a shade of this skin; only real colours (hair, tinted glasses) stay as drawn
                vec3 front = (hmx - hmn) < 0.14 ? skinC * smoothstep(0.02, 0.92, hmx) : hs.rgb;
                float fa = hs.a * smoothstep(-0.2, 0.2, dir.z) * (1.0 - smoothstep(0.62, 0.9, dir.y));      // front half: face; the top (seen from above) and the back: hair / skin
                vec3 back = isoHeadCell.w < 0.0 || isoHairTint.a < 0.5 ? skinC : isoHairTint.rgb;      // bald (cell.w < 0): skin all round
                vec3 headC = mix(back, front, fa);
                albedo = mix(albedo, headC, inHead);
            }
        }
        // cel shading: quantise the light intensity into soft bands
        float L = clamp(dot(lit.rgb, vec3(0.333)), 0.0, 1.6);
        float bands = 3.0;
        float edgeW = fwidth(L * bands) + 0.02;
        float q = (floor(L * bands) + smoothstep(0.5 - edgeW, 0.5 + edgeW, fract(L * bands))) / bands;
        float shade = mix(L, q, isoToon);
        shade = 0.55 + 0.6 * shade;                            // keep shadows readable, never black
        vec3 hue = lit.rgb / max(dot(lit.rgb, vec3(0.333)), 0.001);
        vec3 lightCol = mix(vec3(1.0), clamp(hue, 0.0, 2.0), 0.35);

        // rim light for chunky toy-like silhouettes
        vec3 E = normalize(-v);
        float rim = pow(1.0 - clamp(dot(normalize(N), E), 0.0, 1.0), 3.0);
        // night-time grading: cool moonlit shadows, warm lamp-lit highlights
        vec3 tone = mix(vec3(0.92, 0.93, 0.97), vec3(0.98, 0.96, 0.92), clamp(L, 0.0, 1.0));
        vec3 rgb = albedo * shade * lightCol * tone + rim * vec3(0.10, 0.14, 0.24);

        finalColor = vec4(rgb * finalColor.rgb, finalColor.a * tex.a);
    }

    float magnitude = 1.0;
    if( blackAndWhite == 1 ) magnitude = 0.1;
    if( magnitude < 0.99 )
    {
        finalColor.r = float( int( finalColor.r * bitDepth )) / bitDepth;
        finalColor.g = float( int( finalColor.g * bitDepth )) / bitDepth;
        finalColor.b = float( int( finalColor.b * bitDepth )) / bitDepth;
        float average = (finalColor.r + finalColor.g + finalColor.b) / 3.0;
        average *= 0.85;
        finalColor.rgb = finalColor.rgb * magnitude + vec3(average) * (1.0-magnitude);
    }

    if ( renderFow == 1 )
    {
        vec4 observerSample = texture2D( observers, vec2(currentPosition.x, 1.0-currentPosition.y) );
        vec4 visMapSample = (texture2D( vismap, currentPosition ) * vismapBlend)
                          + (texture2D( vismapPrevious, currentPosition ) * ( 1.0 - vismapBlend ));
        if( visMapSample[3] > 0.0 )
        {
            float m = 1.0 - visMapSample[3];
            m += observerSample[0];
            if( m > 1.0 ) m = 1.0;
            float average = (finalColor.r + finalColor.g + finalColor.b) / 3.0;
            average *= 0.5;
            finalColor.rgb = finalColor.rgb * m + vec3(average) * (1.0-m);
        }
    }
    // Blueprint look for ghost objects (build mode draws planned objects translucent): holographic blue with a bright outline
    if (isoEnabled == 1 && gl_Color.a < 0.98 && isoIsWall < 0.5) {
        float brim = pow(1.0 - clamp(dot(normalize(N), normalize(-v)), 0.0, 1.0), 2.0);
        float grid = 0.5 + 0.5 * step(0.5, fract(isoWorld.x * 4.0)) * step(0.5, fract(isoWorld.y * 4.0));
        gl_FragColor = vec4(vec3(0.22, 0.56, 1.0) * (0.55 + 0.65 * brim) * (0.85 + 0.15 * grid), clamp(0.28 + 0.6 * brim, 0.0, 0.9));
        return;
    }
    if (isoCellsDark > 0.0 && isoEnabled == 1) {      // lights off in cells at Sleep: everything inside a cell room (room map a = 255) goes dark
        vec2 cp = isoWorld.xy;      // a wall face looks at the tile it faces: a cell's inner walls go dark with it, the corridor side does not
        if (isoIsWall > 0.5 && length(isoWN.xy) > 0.5) cp += normalize(isoWN.xy) * 0.45;
        float inCell = step(0.9, texture2D(isoOutdoors, (floor(cp) + 0.5) / worldSize).a);
        finalColor.rgb *= mix(1.0, 0.70, isoCellsDark * inCell);
    }
    // 1.9.108 (user: darker outlines on objects, like the 2D art): where the surface normal turns sharply between neighbouring pixels (a box
    // edge, a crease) the pixel is darkened - thin drawn lines along objects' edges at no extra draw cost. Walls and people keep their look.
    if (isoEnabled == 1 && isoOutline > 0.0 && isoIsWall < 0.5) {
        // 1.9.112 (user: thicker lines help, slightly thicker): a lower crease threshold catches the softer edges too, and faces turned
        // almost edge-on to the camera darken as well, which widens the line along silhouettes of rounded things
        float e = length(fwidth(N));
        float rim = 1.0 - abs(dot(normalize(N), normalize(-v)));
        float line = max(smoothstep(0.22, 0.75, e), 0.75 * smoothstep(0.72, 0.97, rim));
        finalColor.rgb *= 1.0 - isoOutline * line;
    }
    gl_FragColor = finalColor;
}
)GLSL";

// ---------------------------------------------------------------------------
// Injection for the LightMap* shader family (floors, 2D sprites and the
// extruded "pseudo-3D" walls). We splice code into the game's own source so
// every variant (NoFOW / NoTexture / NoColour ...) keeps its behaviour.
// ---------------------------------------------------------------------------
static const char* kLmVsHeader = R"GLSL(
uniform int   isoEnabled;
uniform int   isoWallMode;
uniform int   isoCutStyle;
uniform vec2  isoFocus;
uniform vec2  isoCamDir;
uniform float isoCutRadius;
uniform float isoStub;
varying vec3  isoW;
varying float isoCutL;
vec4 isoCutVertex(vec4 vtx)
{
    isoW = vtx.xyz;
    isoCutL = 0.0;
    if (isoEnabled == 1 && isoWallMode > 0 && abs(vtx.z) > 0.0001)
    {
        float cut = 1.0;
        if (isoWallMode == 1)
        {
            vec2 rel = vtx.xy - isoFocus;
            float along = dot(rel, isoCamDir);
            float lateral = length(rel - along * isoCamDir);
            cut = smoothstep(-0.5, 1.5, along);
            if (isoCutRadius > 0.0) cut *= 1.0 - smoothstep(isoCutRadius, isoCutRadius + 2.0, lateral);
        }
        isoCutL = cut;
        if (isoCutStyle == 0) vtx.z *= mix(1.0, isoStub, cut);
    }
    return vtx;
}
)GLSL";

static const char* kLmFsHeader = R"GLSL(
uniform int   isoEnabled;
uniform int   isoCartoon;
uniform int   isoCutStyle;
uniform float isoGrit;
uniform float isoSat;
uniform float isoWallHeight;
uniform float isoWarm;      // warm, softer lighting: 0 = off
uniform vec3  isoWallTint;
varying vec3  isoW;
varying float isoCutL;
const float isoStyleL = 1.0;

// Softened tileset sample: floors and flat sprites lose their photographic grain (alpha is kept sharp).
vec4 isoTex(vec2 uv)
{
    vec4 s = texture2D(sampler0, uv);
    if (isoEnabled != 1 || isoCartoon != 1) return s;
    vec2 r = fwidth(uv) * 0.9;
    vec3 b = (texture2D(sampler0, uv + vec2(r.x, 0.0)).rgb + texture2D(sampler0, uv - vec2(r.x, 0.0)).rgb +
              texture2D(sampler0, uv + vec2(0.0, r.y)).rgb + texture2D(sampler0, uv - vec2(0.0, r.y)).rgb) * 0.25;
    return vec4(mix(s.rgb, b, 0.55), s.a);
}

float isoLHash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float isoLNoise(vec2 p)
{
    vec2 i = floor(p); vec2 f = fract(p); f = f*f*(3.0-2.0*f);
    return mix(mix(isoLHash(i), isoLHash(i+vec2(1.0,0.0)), f.x),
               mix(isoLHash(i+vec2(0.0,1.0)), isoLHash(i+vec2(1.0,1.0)), f.x), f.y);
}
vec3 isoLSat(vec3 c, float s) { float l = dot(c, vec3(0.299,0.587,0.114)); return clamp(mix(vec3(l), c, s), 0.0, 1.0); }

vec4 isoStylize(vec4 c)
{
    if (isoEnabled != 1) return c;
    // dither fade for cut walls
    if (isoCutStyle == 1 && isoCutL > 0.01 && abs(isoW.z) > 0.15 * isoWallHeight) {
        vec2 fc = floor(mod(gl_FragCoord.xy, 4.0));
        float bayer = mod(fc.x * 3.0 + fc.y * 2.0 + fc.x * fc.y * 5.0, 16.0) / 16.0;
        if (bayer > 1.0 - 0.85 * isoCutL) discard;
    }
    if (isoCartoon != 1) return c;

    vec3 dx = dFdx(isoW), dy = dFdy(isoW);
    vec3 n = cross(dx, dy);
    float nl = length(n);
    n = nl > 1e-9 ? n / nl : vec3(0.0, 0.0, 1.0);
    bool raised = abs(isoW.z) > 0.0005;
    bool side = raised && abs(n.z) < 0.5;

    // natural ground keeps most of its own colour (the strong boost made dirt and grass look cartoon-flat)
    vec3 rgb = isoLSat(c.rgb, raised ? isoSat : 1.0 + (isoSat - 1.0) * 0.25);
    if (!raised) {
        // the game's flat wall-base art (tileset cell 6,14 = "Building") peeks out beside the 3D walls:
        // repaint it as plain concrete. Tileset is 64 x 64 tiles; v may be stored flipped.
        vec2 tt = floor(gl_TexCoord[0].xy * 64.0);
        if (tt.x == 6.0 && (tt.y == 14.0 || tt.y == 49.0)) {
            float g = dot(rgb, vec3(0.299, 0.587, 0.114));
            rgb = vec3(g * 1.1, g * 1.06, g * 1.0) * 0.8 + 0.08;
        }
    }
    if (side)
    {
        // painted blocks on wall faces, colour taken from the wall's own art
        float h = abs(isoW.z);
        float wh = max(isoWallHeight, 0.05);
        float along = abs(n.x) > abs(n.y) ? isoW.y : isoW.x;
        vec3 base = mix(rgb, vec3(0.93, 0.91, 0.86), 0.22) * isoWallTint;
        float rowH = wh / 5.0;
        float row = floor(h / rowH);
        float bw = rowH * 2.2;
        float off = mod(row, 2.0) * 0.5 * bw;
        vec2 cell = vec2(fract((along + off) / bw), fract(h / rowH));
        float edge = min(min(cell.x, 1.0 - cell.x) * bw / rowH, min(cell.y, 1.0 - cell.y));
        float aa = fwidth(h / rowH) * 1.5 + 0.001;
        base *= 1.0 - 0.12 * (1.0 - smoothstep(0.05, 0.05 + aa, edge));
        base *= 0.95 + 0.1 * isoLHash(vec2(floor((along + off) / bw), row));
        float hn = h / wh;
        base *= 1.0 - 0.22 * (1.0 - smoothstep(0.0, 0.3, hn));       // contact shadow
        float stain = isoLNoise(vec2(along * 1.3, hn * 2.5) + isoW.xy * 0.21);
        base *= 1.0 - isoGrit * 0.2 * smoothstep(0.55, 0.9, stain);
        float streak = smoothstep(0.82, 0.97, isoLNoise(vec2(along * 7.0, 0.0))) * smoothstep(0.3, 1.0, hn);
        base = mix(base, base * vec3(0.72, 0.58, 0.45), isoGrit * 0.45 * streak);
        rgb = base;
    }
    else if (raised)
    {
        rgb = vec3(0.84, 0.84, 0.82) * (0.94 + 0.12 * isoLNoise(isoW.xy * 6.0)); // dark slate coping
    }

    if (!raised)
    {
        // floors and flat sprites: iron out photographic noise into soft painted tone bands,
        // then grade cool in the shadows / warm in the lights to match the walls
        float l = dot(rgb, vec3(0.299, 0.587, 0.114));
        float b = 7.0;
        float lw = fwidth(l * b) + 0.05;
        float ql = (floor(l * b) + smoothstep(0.5 - lw, 0.5 + lw, fract(l * b))) / b;
        rgb *= mix(1.0, ql / max(l, 0.004), 0.5 * isoStyleL);
        l = dot(rgb, vec3(0.299, 0.587, 0.114));
        float nv = isoLNoise(isoW.xy * 0.11) * 0.6 + isoLNoise(isoW.xy * 0.37) * 0.4;   // breaks up the tiling of big fields
        rgb *= 0.90 + 0.20 * nv;
        rgb *= mix(vec3(0.94, 0.95, 0.99), vec3(0.97, 0.95, 0.91), clamp(l * 1.6, 0.0, 1.0));
        rgb += (1.0 - rgb) * 0.05 * (1.0 - l);                       // lift crushed blacks
    }

    // cartoon key light from upper-left, fixed in world space
    vec3 L = normalize(vec3(-0.45, -0.6, -0.66));
    float d = dot(n, L);
    if (n.z > 0.0) d = dot(-n, L);                                    // face orientation guard
    float shade = side ? (0.68 + 0.42 * smoothstep(-0.3, 0.6, abs(d))) : 1.0;
    rgb *= shade;
    // gentle contrast curve
    rgb = clamp((rgb - 0.5) * mix(1.08, 0.98, isoWarm) + 0.52, 0.0, 1.0);
    // duller and grittier: pull colour towards grey, darken a touch, add fine film grain
    float lumg = dot(rgb, vec3(0.299, 0.587, 0.114));
    rgb = mix(vec3(lumg), rgb, 0.86) * 0.93;
    rgb += (isoLHash(gl_FragCoord.xy) - 0.5) * 0.045;
    rgb *= mix(vec3(1.0), vec3(1.0, 0.90, 0.76), isoWarm);          // orange light
    return vec4(clamp(rgb, 0.0, 1.0), c.a);
}
)GLSL";
