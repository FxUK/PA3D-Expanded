
#extension GL_ARB_shader_texture_lod : enable
uniform sampler2D isoMat;
uniform sampler2D lightmap;
uniform sampler2D isoCellA;      // per cell: tileset uv at the cell's corner (xy) and one tile along x (zw)
uniform sampler2D isoCellB;      // per cell: uv one tile along y (xy), b = mapped
uniform sampler2D isoTiles;      // the game's tileset
uniform float haveCells;
uniform float tileSoft;         // mip bias: softer, low-poly look
uniform float tileFlat;         // how much each tile is pulled towards its own average colour
uniform vec2  worldSize;
uniform float haveLight;
uniform float floorBright;      // < 1 tones down near-white floors (white tile)
uniform float isoWarm;
uniform sampler2D isoRooms;      // room map (a = 255: a cell)
uniform float isoCellsDark;      // 0..1: cells' lights off (Sleep regime)
uniform sampler2D isoBulbs;      // lamp pools: r = indoor bulbs, g = outdoor lamps, b = near an indoor bulb
uniform float isoNight;          // 0 day .. 1 full night
uniform float isoBulbOn;
varying vec2 wp;

// The light on a spot: the game's lightmap, a moonlit floor at night instead of the old fixed grey, and each lamp's own pool.
vec3 isoNightLight(vec3 lm, vec2 p, float indoor, float dayMin)
{
    vec3 lmv = mix(vec3(dot(lm, vec3(0.333))), lm, 0.5);
    vec3 moon = vec3(0.13, 0.16, 0.26);
    vec3 L = max(lmv, mix(vec3(dayMin, dayMin, dayMin + 0.06), moon, isoNight));
    if (isoBulbOn > 0.5) {
        vec4 b = texture2D(isoBulbs, p / worldSize);
        float cov = b.b * step(0.5, indoor);
        vec3 bulb = mix(vec3(1.0), vec3(1.05, 0.98, 0.88), b.r);      // warm incandescent under the bulb
        L *= mix(vec3(1.0), (0.84 + 0.24 * b.r) * bulb, cov);          // many bulbs, not one even panel: brighter under each, dimmer between
        float o = (1.0 - step(0.5, indoor)) * isoNight;
        L = L * (1.0 - 0.35 * o * (1.0 - b.g)) + o * b.g * vec3(1.0, 0.85, 0.60) * 0.80;      // outdoor lamps light real pools on the dark ground
    }
    L = min(L, vec3(1.0));      // 1.9.76: never brighter than the game's full light (user: overexposed)
    return L;
}

float h21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2  h22(vec2 p) { return vec2(h21(p), h21(p + vec2(17.3, 5.1))); }
float vn(vec2 p) { vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
    return mix(mix(h21(i), h21(i + vec2(1.0, 0.0)), f.x), mix(h21(i + vec2(0.0, 1.0)), h21(i + vec2(1.0, 1.0)), f.x), f.y); }
float fbm(vec2 p) { return vn(p) * 0.55 + vn(p * 2.03 + 7.1) * 0.30 + vn(p * 4.1 + 3.7) * 0.15; }
float matAt(vec2 cell) { return floor(texture2D(isoMat, (cell + 0.5) / worldSize).r * 255.0 + 0.5); }

vec3 tilesF(vec2 p, float s, vec3 a, vec3 b, vec3 gr) {
    vec2 q = p / s; vec2 id = floor(q); vec2 f = fract(q);
    float g = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
    float ln = 1.0 - smoothstep(0.015, 0.05, g);
    vec3 c = mix(a, b, h21(id) * 0.7 + 0.3 * fbm(p * 2.0)); c *= 0.95 + 0.10 * fbm(p * 8.0);
    return mix(c, gr, ln * 0.75);
}
vec3 planksF(vec2 p, vec3 a, vec3 b) {
    float row = floor(p.y / 0.25); float off = h21(vec2(row, 3.0)) * 3.0;
    float u = (p.x + off) / 1.2; float id = floor(u); float fx = fract(u); float fy = fract(p.y / 0.25);
    float ln = (1.0 - smoothstep(0.0, 0.04, fx)) + (1.0 - smoothstep(0.0, 0.08, fy));
    vec3 c = mix(a, b, h21(vec2(id, row))); c *= 0.92 + 0.16 * vn(vec2(p.x * 3.0, p.y * 40.0));
    return mix(c, c * 0.55, clamp(ln, 0.0, 1.0) * 0.6);
}
vec3 cobblesF(vec2 p, vec3 a, vec3 b) {
    vec2 q = p * 2.0; vec2 i = floor(q); vec2 f = fract(q);
    float d1 = 9.0, d2 = 9.0; vec2 id = vec2(0.0);
    for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
        vec2 g = vec2(float(x), float(y)); vec2 o = h22(i + g); vec2 r = g + o - f; float d = dot(r, r);
        if (d < d1) { d2 = d1; d1 = d; id = i + g; } else if (d < d2) d2 = d;
    }
    float edge = sqrt(d2) - sqrt(d1); vec3 c = mix(a, b, h21(id)); c *= 0.9 + 0.2 * fbm(p * 6.0);
    return mix(c * 0.5, c, smoothstep(0.0, 0.12, edge));
}
vec3 grassF(vec2 p) { float n = fbm(p * 1.3); float blade = vn(vec2(p.x * 14.0, p.y * 3.0));
    return mix(vec3(0.34, 0.60, 0.15), vec3(0.50, 0.74, 0.20), n) * (0.88 + 0.20 * blade); }
vec3 dirtF(vec2 p) { float n = fbm(p * 1.1); vec3 c = mix(vec3(0.50, 0.35, 0.20), vec3(0.64, 0.46, 0.26), n); return c * (0.90 + 0.20 * vn(p * 9.0)); }
vec3 concreteF(vec2 p) { return vec3(0.58, 0.57, 0.55) * (0.90 + 0.18 * fbm(p * 3.0)); }

bool soft(float m) { return m < 22.5 || (m > 29.5 && m < 30.5); }

vec3 floorColour(float m, vec2 p) {
    if (m > 7.5 && m < 8.5) return dirtF(p);
    if (m > 8.5 && m < 9.5) return mix(vec3(0.88, 0.91, 0.95), vec3(0.97, 0.98, 1.0), fbm(p * 2.0));
    if (m > 9.5 && m < 10.5) return tilesF(p, 0.5, vec3(0.50, 0.47, 0.50), vec3(0.62, 0.58, 0.60), vec3(0.30, 0.29, 0.31));
    if (m > 10.5 && m < 11.5) return grassF(p);
    if (m > 11.5 && m < 12.5) return grassF(p * 0.8) * vec3(0.86, 0.92, 0.72);
    if (m > 12.5 && m < 13.5) return dirtF(p * 0.7) * vec3(0.58, 0.50, 0.42);
    if (m > 13.5 && m < 14.5) return cobblesF(p * 1.6, vec3(0.55, 0.52, 0.46), vec3(0.66, 0.62, 0.55));
    if (m > 14.5 && m < 15.5) return vec3(0.27, 0.27, 0.29) * (0.9 + 0.2 * fbm(p * 5.0));
    if (m > 15.5 && m < 21.5) { vec3 r = vec3(0.27, 0.27, 0.29) * (0.9 + 0.2 * fbm(p * 5.0)); float l = step(abs(fract(p.x * 2.0) - 0.5), 0.07) * step(0.2, fract(p.y * 2.0)); return mix(r, vec3(0.92, 0.9, 0.7), l * 0.6); }
    if (m > 21.5 && m < 22.5) return cobblesF(p, vec3(0.50, 0.50, 0.52), vec3(0.62, 0.62, 0.64));
    if (m > 23.5 && m < 24.5) return tilesF(p, 0.5, vec3(0.62, 0.61, 0.60), vec3(0.70, 0.69, 0.67), vec3(0.38, 0.37, 0.36));
    if (m > 24.5 && m < 25.5) return concreteF(p);
    if (m > 25.5 && m < 26.5) return planksF(p, vec3(0.66, 0.44, 0.22), vec3(0.78, 0.55, 0.30));
    if (m > 26.5 && m < 27.5) return tilesF(p, 0.5, vec3(0.90, 0.88, 0.84), vec3(0.97, 0.95, 0.91), vec3(0.62, 0.61, 0.58));
    if (m > 27.5 && m < 28.5) return tilesF(p, 0.25, vec3(0.30, 0.46, 0.62), vec3(0.78, 0.74, 0.62), vec3(0.85, 0.85, 0.85));
    if (m > 28.5 && m < 29.5) return tilesF(p, 0.5, vec3(0.50, 0.53, 0.57), vec3(0.58, 0.61, 0.65), vec3(0.30, 0.32, 0.35));
    if (m > 29.5 && m < 30.5) return mix(vec3(0.82, 0.72, 0.50), vec3(0.90, 0.80, 0.58), fbm(p * 2.5)) * (0.94 + 0.08 * vn(p * 20.0));
    if (m > 30.5 && m < 31.5) return tilesF(p, 1.0, vec3(0.90, 0.89, 0.87), vec3(0.97, 0.96, 0.94), vec3(0.70, 0.69, 0.68));
    if (m > 31.5 && m < 32.5) return tilesF(p, 0.5, vec3(0.93, 0.93, 0.92), vec3(0.98, 0.98, 0.97), vec3(0.72, 0.72, 0.72));
    if (m > 32.5 && m < 33.5) { vec3 c = tilesF(p, 0.5, vec3(0.78, 0.66, 0.50), vec3(0.88, 0.76, 0.58), vec3(0.45, 0.35, 0.25)); return c; }
    if (m > 33.5 && m < 34.5) return tilesF(p, 0.5, vec3(0.36, 0.39, 0.43), vec3(0.45, 0.48, 0.52), vec3(0.20, 0.22, 0.25));
    if (m > 34.5 && m < 35.5) return tilesF(p, 0.5, vec3(0.45, 0.44, 0.40), vec3(0.54, 0.52, 0.47), vec3(0.25, 0.25, 0.22));
    if (m > 35.5 && m < 36.5) { vec2 c = floor(p * 2.0); float ck = mod(c.x + c.y, 2.0); return mix(vec3(0.88, 0.86, 0.80), vec3(0.34, 0.36, 0.40), ck) * (0.94 + 0.10 * fbm(p * 7.0)); }
    if (m > 39.5 && m < 40.5) return tilesF(p, 0.5, vec3(0.93, 0.93, 0.92), vec3(0.98, 0.98, 0.97), vec3(0.72, 0.72, 0.72));   // ids 40/41 are big indoor floors in real maps (kitchen), not water
    if (m > 40.5 && m < 41.5) return tilesF(p, 0.5, vec3(0.50, 0.53, 0.57), vec3(0.58, 0.61, 0.65), vec3(0.30, 0.32, 0.35));
    if ((m > 36.5 && m < 39.5) || (m > 41.5 && m < 254.5 && false) || m > 254.5) {   // wall cells (learned per save: marked 255): a darker brick footing, so the strip beside the thin wall reads as part of it
        vec2 bq = p * vec2(2.0, 4.0); float brw = floor(bq.y); float bo = mod(brw, 2.0) * 0.5;
        float bx = fract(bq.x + bo), by = fract(bq.y);
        float ml = clamp(step(by, 0.12) + step(bx, 0.07), 0.0, 1.0);
        vec3 bc = vec3(0.40, 0.36, 0.32) * (0.85 + 0.25 * h21(vec2(floor(bq.x + bo), brw)));
        return mix(bc, vec3(0.24, 0.22, 0.20), ml * 0.8);
    }
    return concreteF(p);          // anything else: plain concrete
}

void main(void)
{
    vec2 cell = floor(wp); vec2 f = wp - cell;
    float m = matAt(cell);
    vec2 src = cell;
    bool wallUnder = false; vec3 wallCol = vec3(0.0);
    if (m > 254.5 && haveCells > 0.5) {      // 1.9.113 (user: the strip under a thinner wall should match the wall, not the next tile): the wall's own
        vec4 wb = texture2D(isoCellB, (cell + 0.5) / worldSize);      // colour, from its 2D art's average (fences keep the floor: their art is see-through)
        if (wb.b > 0.5) {
            vec4 wa = texture2D(isoCellA, (cell + 0.5) / worldSize);
            bool fence = wa.x < 0.07 && wa.y > 0.695 && wa.y < 0.765;
            if (!fence) { wallUnder = true; wallCol = texture2D(isoTiles, wa.xy + 0.5 * (wa.zw - wa.xy) + 0.5 * (wb.xy - wa.xy), 7.0).rgb; }
        }
    }
    if (m > 254.5 && !wallUnder) {      // a wall tile: the floor runs on under the wall - the nearest neighbouring floor (sides first, then corners), never the wall's 2D art
        // (1.9.97, user: the wall tiles' own art / brick footing looked worse than the surrounding floor)
        vec2 nb[8]; nb[0] = vec2(1.0, 0.0); nb[1] = vec2(-1.0, 0.0); nb[2] = vec2(0.0, 1.0); nb[3] = vec2(0.0, -1.0);
        nb[4] = vec2(1.0, 1.0); nb[5] = vec2(-1.0, 1.0); nb[6] = vec2(1.0, -1.0); nb[7] = vec2(-1.0, -1.0);
        bool found = false;
        for (int k = 0; k < 8; k++) {      // first a neighbour with the game's own floor art
            vec2 c2 = cell + nb[k];
            if (!found && haveCells > 0.5 && matAt(c2) < 254.5 && texture2D(isoCellB, (c2 + 0.5) / worldSize).b > 0.5) { src = c2; m = matAt(c2); found = true; }
        }
        for (int k = 0; k < 8; k++) {      // else any neighbouring floor material
            vec2 c2 = cell + nb[k];
            if (!found && matAt(c2) < 254.5) { src = c2; m = matAt(c2); found = true; }
        }
        if (!found) m = 25.0;      // walls all round: plain concrete
    }
    vec4 cb = texture2D(isoCellB, (src + 0.5) / worldSize);
    bool mapped = haveCells > 0.5 && cb.b > 0.5 && m < 254.5;
    if (!mapped && m > 14.5 && m < 21.5) discard;     // roads and their markings: leave the game's own terrain (it has the correct centre dashes)
    vec3 col;
    if (mapped) {      // the game's own 2D floor art for this cell, softened a little
        vec4 ca = texture2D(isoCellA, (src + 0.5) / worldSize);
        vec2 uv = ca.xy + f.x * (ca.zw - ca.xy) + f.y * (cb.xy - ca.xy);
        vec2 ex = ca.zw - ca.xy, ey = cb.xy - ca.xy; float sc = exp2(tileSoft);
#ifdef GL_ARB_shader_texture_lod
        vec2 dwx = dFdx(wp), dwy = dFdy(wp);      // gradients from the continuous world position: no seams where one cell's mapping meets the next
        vec3 t = texture2DGradARB(isoTiles, uv, (ex * dwx.x + ey * dwx.y) * sc, (ex * dwy.x + ey * dwy.y) * sc).rgb;
#else
        vec3 t = texture2D(isoTiles, uv, tileSoft).rgb;
#endif
        vec3 avg = texture2D(isoTiles, ca.xy + 0.5 * (ca.zw - ca.xy) + 0.5 * (cb.xy - ca.xy), 7.0).rgb;
        col = mix(t, avg, tileFlat);
    } else if (wallUnder) col = wallCol * 1.15 * (floorColour(255.0, wp) / vec3(0.40, 0.36, 0.32));      // the wall's colour with the footing's brick courses
    else col = floorColour(m, wp);
    if (!mapped && soft(m)) {
        float wob = 0.22 + 0.18 * fbm(wp * 3.0);
        float mr = matAt(cell + vec2(1.0, 0.0)); float ml = matAt(cell - vec2(1.0, 0.0));
        float mu = matAt(cell + vec2(0.0, 1.0)); float md = matAt(cell - vec2(0.0, 1.0));
        if (mr != m && soft(mr)) col = mix(col, floorColour(mr, wp), (1.0 - smoothstep(0.0, wob, 1.0 - f.x)) * 0.5);
        if (ml != m && soft(ml)) col = mix(col, floorColour(ml, wp), (1.0 - smoothstep(0.0, wob, f.x)) * 0.5);
        if (mu != m && soft(mu)) col = mix(col, floorColour(mu, wp), (1.0 - smoothstep(0.0, wob, 1.0 - f.y)) * 0.5);
        if (md != m && soft(md)) col = mix(col, floorColour(md, wp), (1.0 - smoothstep(0.0, wob, f.y)) * 0.5);
    }
    // slow, large-scale variation so wide areas do not look stamped
    col *= 0.93 + 0.14 * fbm(wp * 0.09);
    if (haveLight > 0.5) {
        vec3 lm = texture2D(lightmap, wp / worldSize).rgb;
        float indoor = texture2D(isoRooms, (floor(wp) + 0.5) / worldSize).r;
        vec3 L = isoNightLight(lm, wp, indoor, 0.52);
        L = mix(L, L * vec3(1.06, 0.98, 0.86), isoWarm);                       // the pools of light turn orange
        col *= L;
    }
    float lumf = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(lumf), col, 0.86) * 0.93;            // duller
    col *= mix(1.0, floorBright, smoothstep(0.55, 0.85, dot(col, vec3(0.299, 0.587, 0.114))));      // near-white tiles are too bright: pull them down, leave dark floors alone
    col += (h21(gl_FragCoord.xy) - 0.5) * 0.045;        // grittier: fine grain
    col *= mix(vec3(1.0), vec3(1.0, 0.91, 0.78), isoWarm);
    if (isoCellsDark > 0.0) col *= mix(1.0, 0.70, isoCellsDark * step(0.9, texture2D(isoRooms, (floor(wp) + 0.5) / worldSize).a));      // lights off in cells at Sleep
    gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
