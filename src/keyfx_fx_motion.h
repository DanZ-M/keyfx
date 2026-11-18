/*
 * Motion effects for keyfx: beam, snake, spiral, ball, lightning, swipe and matrix.
 *
 * This file is included by keyfx_engine.c (it uses the engine's helpers, so it is not a
 * stand-alone header). Everything here is integer maths: angles are degrees, directions are
 * vectors scaled by 1024, positions are tenths of a key pitch (like CI_KEYS).
 *
 * Angles: 0 = to the right, 90 = down, 180 = left, 270 = up.
 */

static int16_t lx_min, lx_max, ly_min, ly_max;
#define ABSV(v) ((v) < 0 ? -(v) : (v))

/* ---- small maths ---- */

static const int16_t sin_tab[19] = {0,   89,  178, 265, 350, 433, 512, 587, 658, 724,
                                    784, 839, 887, 928, 962, 989, 1008, 1020, 1024};

static int32_t sin1024(int32_t deg) {
    int32_t sign = 1;

    deg %= 360;
    if (deg < 0) {
        deg += 360;
    }
    if (deg >= 180) {
        deg -= 180;
        sign = -1;
    }
    if (deg > 90) {
        deg = 180 - deg;
    }
    int32_t i = deg / 5, r = deg % 5;
    int32_t v = i >= 18 ? 1024 : sin_tab[i] + ((sin_tab[i + 1] - sin_tab[i]) * r) / 5;

    return sign * v;
}

static int32_t cos1024(int32_t deg) {
    return sin1024(deg + 90);
}

/* How far an effect has got, 0..1024. accel -100..100: + starts slow and ends fast,
 * - starts fast and ends slow. */
static int32_t fx_prog(const struct ci_fx_def *d, int32_t t) {
    int32_t dur = d->duration_ms;

    if (dur <= 0) {
        return 1024;
    }
    int32_t x = (CLAMP(t, 0, dur) * 1024) / dur;
    int32_t p = x + ((int32_t)d->accel * (((x * x) >> 10) - x)) / 100;

    return CLAMP(p, 0, 1024);
}

/* fade out during the last tenth of the effect */
static uint32_t fx_tail(const struct ci_fx_def *d, int32_t t) {
    int32_t left = d->duration_ms - t;
    int32_t span = d->duration_ms / 10;

    return (span > 0 && left < span) ? (uint32_t)((255 * MAX(left, 0)) / span) : 255;
}

/* ---- where, which way, how far ---- */

static void fx_start_pt(const struct ci_fx_def *d, const struct fx_inst *f, int32_t *x,
                        int32_t *y) {
    if (d->from) {
        int16_t px, py;

        pos_xy(d->from - 1, &px, &py);
        *x = px;
        *y = py;
    } else {
        *x = f->ox;
        *y = f->oy;
    }
}

/* base direction in degrees: random, towards the end key, or the angle you set */
static int32_t fx_base_angle(const struct ci_fx_def *d, const struct fx_inst *f, int32_t sx,
                             int32_t sy) {
    if (d->random) {
        return hash32(f->seed ^ 0x51EDu) % 360;
    }
    if (d->to) {
        int16_t tx, ty;
        int32_t best = d->angle, bestdot = INT32_MIN;

        pos_xy(d->to - 1, &tx, &ty);
        for (int32_t a = 0; a < 360; a += 5) {
            int32_t dot = (tx - sx) * cos1024(a) + (ty - sy) * sin1024(a);

            if (dot > bestdot) {
                bestdot = dot;
                best = a;
            }
        }
        return best;
    }
    return d->angle;
}

static int32_t fx_dist(const struct ci_fx_def *d, int32_t sx, int32_t sy, int32_t dflt) {
    if (d->dist) {
        return d->dist;
    }
    if (d->to) {
        int16_t tx, ty;

        pos_xy(d->to - 1, &tx, &ty);
        int32_t len = isqrt32((uint32_t)((tx - sx) * (tx - sx) + (ty - sy) * (ty - sy)));

        if (len > 0) {
            return len;
        }
    }
    return dflt;
}

/* angle of shot k of a fan: `count` shots spread over `spread` degrees around the base */
static int32_t shot_angle(const struct ci_fx_def *d, int32_t base, int32_t k) {
    if (d->count > 1) {
        return base - (int32_t)d->spread / 2 + ((int32_t)d->spread * k) / (d->count - 1);
    }
    return base;
}

/* ---- a head that moves along a path, dragging a trail (beam, snake, spiral, ball) ---- */

struct mv {
    int32_t sx, sy; /* where it starts                      */
    int32_t ang;    /* direction, degrees                   */
    int32_t dist;   /* length of the path, or radius        */
};

/* height of the ball above the floor, `q` = 0..1024 through the whole fall and bounces */
static int32_t ball_height(const struct ci_fx_def *d, int32_t q, int32_t h0) {
    int32_t hk[7], wk[7], total = 0;
    int32_t b = d->bounce ? d->bounce : 60;

    hk[0] = h0;
    wk[0] = (int32_t)isqrt32((uint32_t)h0 * 1024u); /* falling time grows with the square root of height */
    total = wk[0];
    for (int i = 1; i < 7; i++) {
        hk[i] = (hk[i - 1] * b * b) / 10000;
        wk[i] = 2 * (int32_t)isqrt32((uint32_t)hk[i] * 1024u);
        total += wk[i];
    }
    if (total <= 0) {
        return 0;
    }
    int32_t s = (q * total) / 1024;

    if (s < wk[0]) {
        int32_t u = (s * 1024) / wk[0];

        return (h0 * (1024 - ((u * u) >> 10))) >> 10;
    }
    s -= wk[0];
    for (int i = 1; i < 7; i++) {
        if (s < wk[i]) {
            int32_t u = (s * 1024) / wk[i];

            return (hk[i] * 4 * ((u * (1024 - u)) >> 10)) >> 10;
        }
        s -= wk[i];
    }
    return 0;
}

static void path_pt(const struct ci_fx_def *d, const struct mv *m, int32_t q, int32_t *x,
                    int32_t *y) {
    int32_t cx = cos1024(m->ang), cy = sin1024(m->ang);

    switch (d->type) {
    case CI_FX_SPIRAL: {
        int32_t turns = d->turns ? d->turns : 2;
        int32_t r = (m->dist * q) >> 10;
        int32_t phi = m->ang + (turns * 360 * q) / 1024;

        *x = m->sx + (r * cos1024(phi)) / 1024;
        *y = m->sy + (r * sin1024(phi)) / 1024;
        return;
    }
    case CI_FX_BALL: {
        int32_t h0 = d->height ? d->height : 60;
        int32_t floor = m->sy + h0;

        *x = m->sx + (((m->dist * q) >> 10) * cx) / 1024;
        *y = floor - ball_height(d, q, h0);
        return;
    }
    default: { /* beam and snake: a straight line, the snake with a sideways wiggle */
        int32_t along = (m->dist * q) >> 10;
        int32_t side = 0;

        if (d->type == CI_FX_SNAKE) {
            side = ((d->wiggle ? d->wiggle : 8) * sin1024(along * 6)) / 1024;
        }
        *x = m->sx + (along * cx) / 1024 - (side * cy) / 1024;
        *y = m->sy + (along * cy) / 1024 + (side * cx) / 1024;
        return;
    }
    }
}

static uint32_t comet_level(const struct ci_fx_def *d, int led, int32_t t, const struct mv *m) {
    const int NTRAIL = 10;
    int32_t trail = ((d->size ? d->size : 25) * 1024) / 100;
    int32_t head = (fx_prog(d, t) * (1024 + trail)) >> 10;
    int32_t width = d->width ? d->width : 14;
    uint32_t best = 0;

    for (int j = 0; j < NTRAIL; j++) {
        int32_t pj = head - (j * trail) / NTRAIL;
        int32_t x, y;

        if (pj < 0) {
            break;
        }
        pj = MIN(pj, 1024);
        path_pt(d, m, d->invert ? 1024 - pj : pj, &x, &y);

        int32_t dx = lx[led] - x, dy = ly[led] - y;
        int32_t d2 = dx * dx + dy * dy;

        if (d2 >= width * width) {
            continue;
        }
        uint32_t spatial = (255 * (uint32_t)(width - (int32_t)isqrt32((uint32_t)d2))) / width;
        uint32_t v = (spatial * (uint32_t)(NTRAIL - j)) / NTRAIL;

        best = MAX(best, v);
    }
    return (best * fx_tail(d, t)) / 255;
}

/* beam, snake, spiral and ball: one or more shots of a comet */
static uint32_t comet_fx(const struct ci_fx_def *d, const struct fx_inst *f, int led, int32_t t) {
    struct mv m;
    int32_t shots = (d->type == CI_FX_BEAM && d->count > 1) ? d->count : 1;
    int32_t base, dflt = (d->type == CI_FX_SPIRAL) ? 70 : 140;
    uint32_t best = 0;

    fx_start_pt(d, f, &m.sx, &m.sy);
    base = fx_base_angle(d, f, m.sx, m.sy);
    m.dist = fx_dist(d, m.sx, m.sy, dflt);
    if (d->type == CI_FX_BALL && !d->angle && !d->to && !d->random) {
        base = 0; /* the ball travels to the right unless you choose */
    }
    for (int32_t k = 0; k < shots; k++) {
        m.ang = shot_angle(d, base, k);
        best = MAX(best, comet_level(d, led, t, &m));
    }
    return best;
}

/* ---- lightning: a jagged bolt that grows quickly, then flickers and fades ---- */

static uint32_t lightning_fx(const struct ci_fx_def *d, const struct fx_inst *f, int led,
                             int32_t t) {
    int32_t sx, sy, dur = d->duration_ms;
    int32_t base, dist, amp = d->wiggle ? d->wiggle : 20;
    int32_t width = d->width ? d->width : 7;
    int32_t bolts = d->count ? d->count : 1;
    uint32_t best = 0;

    fx_start_pt(d, f, &sx, &sy);
    base = d->random ? (int32_t)(hash32(f->seed ^ 0x51EDu) % 360)
                     : (d->angle || d->to ? fx_base_angle(d, f, sx, sy) : 90);
    dist = fx_dist(d, sx, sy, 140);

    int32_t grow = MIN(1024, (t * 1024) / MAX(dur / 5, 1)); /* down in the first fifth */
    int32_t cx = cos1024(base), cy = sin1024(base);
    int32_t dx = lx[led] - sx, dy = ly[led] - sy;
    int32_t along = (dx * cx + dy * cy) / 1024;
    int32_t perp = (-dx * cy + dy * cx) / 1024;

    if (along < 0 || along > (dist * grow) / 1024) {
        return 0;
    }
    /* brightness of this strike and its flicker */
    uint32_t h = hash32(f->seed ^ 0xB017u);
    uint32_t bright = 160 + (h % 96);
    uint32_t slot = hash32(f->seed ^ (uint32_t)(t / MAX(dur / 14, 1)));
    uint32_t flick = (t > dur / 5 && (slot & 3) == 0) ? 70 : 255;

    for (int32_t b = 0; b < bolts; b++) {
        int32_t seg = along / 24, frac = along % 24;
        int32_t o0 = ((int32_t)(hash32(f->seed ^ (uint32_t)(seg * 7919 + b * 104729)) % (2 * amp + 1))) - amp;
        int32_t o1 = ((int32_t)(hash32(f->seed ^ (uint32_t)((seg + 1) * 7919 + b * 104729)) % (2 * amp + 1))) - amp;
        int32_t off = o0 + ((o1 - o0) * frac) / 24;
        int32_t dd = ABSV(perp - off);

        if (dd < width) {
            best = MAX(best, (255 * (uint32_t)(width - dd)) / width);
        }
    }
    return (((best * bright) / 255) * flick / 255) * fx_tail(d, t) / 255;
}

/* ---- swipe: a wall of light sweeping across, in any direction, even from the middle out ---- */

static uint32_t swipe_fx(const struct ci_fx_def *d, const struct fx_inst *f, int led, int32_t t) {
    int32_t sx, sy;

    if (d->bidir && d->from && d->to) { /* the line runs through the middle of the two keys */
        int16_t ax, ay, bx, by;

        pos_xy(d->from - 1, &ax, &ay);
        pos_xy(d->to - 1, &bx, &by);
        sx = (ax + bx) / 2;
        sy = (ay + by) / 2;
    } else if (d->bidir && !d->from) { /* no start key: the middle of the board */
        sx = (lx_min + lx_max) / 2;
        sy = (ly_min + ly_max) / 2;
    } else {
        fx_start_pt(d, f, &sx, &sy);
    }
    int32_t ang = d->bidir ? d->angle : (d->angle || d->to || d->random ? fx_base_angle(d, f, sx, sy) : 0);
    int32_t cx = cos1024(ang), cy = sin1024(ang);
    int32_t s = ((lx[led] - sx) * cx + (ly[led] - sy) * cy) / 1024; /* distance along the sweep */
    int32_t lo = 0, hi = 0;

    /* the range covered: the whole keyboard, or from the start key to the end key / dist */
    int32_t cxs[4] = {lx_min - sx, lx_max - sx, lx_min - sx, lx_max - sx};
    int32_t cys[4] = {ly_min - sy, ly_min - sy, ly_max - sy, ly_max - sy};

    for (int i = 0; i < 4; i++) {
        int32_t pr = (cxs[i] * cx + cys[i] * cy) / 1024;

        lo = i ? MIN(lo, pr) : pr;
        hi = i ? MAX(hi, pr) : pr;
    }
    if (d->bidir) { /* both ways from the line: distance from the line, out to the far end */
        s = ABSV(s);
        hi = MAX(ABSV(hi), ABSV(lo));
        if (d->dist) {
            hi = d->dist;
        }
        lo = 0;
    } else if (d->dist || d->to) {
        lo = 0;
        hi = fx_dist(d, sx, sy, hi);
    }
    int32_t width = d->width ? d->width : 18;
    int32_t p = fx_prog(d, t);

    if (d->invert) {
        p = 1024 - p;
    }
    int32_t front = lo - width + (((hi - lo) + 2 * width) * p) / 1024;
    int32_t diff = front - s; /* > 0: the front has passed this LED */
    uint32_t lvl = 0;

    if (d->fill) { /* everything the front has passed stays lit, then fades with the effect */
        lvl = diff >= 0 ? 255 : (diff > -width ? (uint32_t)(255 * (width + diff)) / width : 0);
    } else if (ABSV(diff) < width) {
        lvl = (255 * (uint32_t)(width - ABSV(diff))) / width;
    }
    return (lvl * fx_tail(d, t)) / 255;
}

/* ---- matrix: columns of falling light with a flickering shine ---- */

/* Matrix: how much of the head colour (.head) an LED shows, 0..255 (set by matrix_fx) */
static uint32_t fx_head_mix;

static uint32_t matrix_fx(const struct ci_fx_def *d, const struct fx_inst *f, int led, int32_t t) {
    int32_t ang = d->random ? (int32_t)(hash32(f->seed) % 360) : (d->angle ? d->angle : 90);
    int32_t cx = cos1024(ang), cy = sin1024(ang);
    int32_t along = (lx[led] * cx + ly[led] * cy) / 1024;
    int32_t perp = (-lx[led] * cy + ly[led] * cx) / 1024;
    int32_t col = (perp + 1000) / 10; /* one column per key pitch across the fall */
    uint32_t h = hash32((uint32_t)col * 2654435761u ^ f->seed);

    /* how many columns rain: between .count and .count_max percent, chosen at every start */
    int32_t dmin = d->count ? d->count : 60;
    int32_t dmax = MAX(d->count_max, dmin);
    int32_t density = dmin + (int32_t)(hash32(f->seed ^ 0xC01Du) % (uint32_t)(dmax - dmin + 1));

    if ((int32_t)(h % 100) >= density) {
        return 0;
    }
    int32_t dur = d->duration_ms;
    int32_t trail = d->dist ? d->dist : 45;
    int32_t amin = 0, amax = 0;
    int32_t xs[4] = {lx_min, lx_max, lx_min, lx_max}, ys[4] = {ly_min, ly_min, ly_max, ly_max};

    for (int i = 0; i < 4; i++) {
        int32_t c = (xs[i] * cx + ys[i] * cy) / 1024;

        amin = (i == 0) ? c : MIN(amin, c);
        amax = (i == 0) ? c : MAX(amax, c);
    }
    /* every column falls at its own speed, between .speed_min and .speed_max percent */
    int32_t smin = d->speed_min ? d->speed_min : 70;
    int32_t smax = MAX(d->speed_max ? d->speed_max : 130, smin);
    int32_t speed = smin + (int32_t)((h >> 16) % (uint32_t)(smax - smin + 1));
    int32_t delay = (int32_t)((h >> 8) % (uint32_t)MAX((dur * 30) / 100, 1));
    int32_t room = (dur * 90) / 100 - delay; /* time left for this column to fall */
    int32_t fall = MIN(((dur * 55) / 100 * 100) / speed, room);
    int32_t tt = t - delay;

    if (tt < 0 || fall <= 0 || tt > fall) {
        return 0;
    }
    int32_t head = amin + (((amax - amin) + trail) * tt) / fall;
    int32_t behind = head - along;

    if (behind < 0 || behind > trail) {
        return 0;
    }
    uint32_t lvl = (255 * (uint32_t)(trail - behind)) / trail;

    /* head colour: the leading light, fading into the trail colour */
    fx_head_mix = 0;
    if (d->head && behind < trail / 3) {
        fx_head_mix = (255 * (uint32_t)(trail / 3 - behind)) / (uint32_t)(trail / 3);
    }
    /* flicker: stronger and faster the further it is from the head, its own rhythm per column */
    int32_t fl = d->flicker ? d->flicker : 40;
    int32_t eff = (fl * (30 + (70 * behind) / trail)) / 100;
    int32_t period = 50 + (int32_t)((h >> 4) % 100);
    uint32_t r = hash32((uint32_t)led * 40503u ^ f->seed ^ (uint32_t)((t + (int32_t)(h & 63)) / period)) % 101;
    uint32_t shine = 255 - (uint32_t)((eff * (int32_t)r) / 100);

    return (((lvl * shine) / 255) * fx_tail(d, t)) / 255;
}

static uint32_t fx_motion_level(const struct ci_fx_def *d, const struct fx_inst *f, int led,
                                int32_t t) {
    switch (d->type) {
    case CI_FX_BEAM:
    case CI_FX_SNAKE:
    case CI_FX_SPIRAL:
    case CI_FX_BALL:
        return comet_fx(d, f, led, t);
    case CI_FX_LIGHTNING:
        return lightning_fx(d, f, led, t);
    case CI_FX_SWIPE:
        return swipe_fx(d, f, led, t);
    case CI_FX_MATRIX:
        return matrix_fx(d, f, led, t);
    default:
        return 0;
    }
}
