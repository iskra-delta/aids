#include "game.h"
#include "platform/platform.h"

#include <stdbool.h>
#include <stdint.h>

#define FP_SHIFT 4
#define FP_ONE (1 << FP_SHIFT)

#define DIR_COUNT 32
#define DIR_MASK (DIR_COUNT - 1)
#define DIR_SCALE 32

#define SHIP_THRUST 2
#define SHIP_MAX_V 28
#define SHIP_FRICTION 1
#define SHIP_FRICTION_DELAY 3
#define SHIP_NOSE 14
#define SHIP_SIDE 10
#define SHIP_TURN_DELAY 4

#define BULLET_MAX 10
#define BULLET_LIFE 40
#define BULLET_SPEED 30
#define FIRE_COOLDOWN 7

#define AST_MAX 24
#define AST_START 6
#define AST_SPEED_BASE 6

struct Bullet {
    int16_t x;
    int16_t y;
    int16_t vx;
    int16_t vy;
    int16_t life;
    uint8_t active;
};

struct Asteroid {
    int16_t x;
    int16_t y;
    int16_t vx;
    int16_t vy;
    int8_t spin;
    uint8_t size;
    uint8_t verts;
    uint8_t angle;
    uint8_t radii[8];
    uint8_t active;
};

struct Game {
    int16_t width_fp;
    int16_t height_fp;
    int16_t ship_x;
    int16_t ship_y;
    int16_t ship_vx;
    int16_t ship_vy;
    int16_t fire_cd;
    int16_t turn_cd;
    int16_t friction_cd;
    int16_t explode_x;
    int16_t explode_y;
    int16_t explode_timer;
    int16_t score;
    int16_t wave;
    uint8_t ship_dir;
    uint8_t lives;
    uint8_t thrusting;
    uint8_t fire_queued;
    uint8_t frame_flip;
    uint8_t running;
    uint8_t over;
    uint16_t rng;
    struct Bullet bullets[BULLET_MAX];
    struct Asteroid asteroids[AST_MAX];
};

static const int8_t k_dir_x[DIR_COUNT] = {
    32, 31, 30, 27, 23, 18, 12, 6, 0, -6, -12, -18, -23, -27, -30, -31,
    -32, -31, -30, -27, -23, -18, -12, -6, 0, 6, 12, 18, 23, 27, 30, 31
};
static const int8_t k_dir_y[DIR_COUNT] = {
    0, -6, -12, -18, -23, -27, -30, -31, -32, -31, -30, -27, -23, -18, -12, -6,
    0, 6, 12, 18, 23, 27, 30, 31, 32, 31, 30, 27, 23, 18, 12, 6
};
static const uint8_t k_angle_idx_6[6] = {0, 6, 10, 16, 22, 26};
static const uint8_t k_angle_idx_7[7] = {0, 4, 10, 14, 18, 24, 28};
static const uint8_t k_angle_idx_8[8] = {0, 4, 8, 12, 16, 20, 24, 28};

static int16_t iabs16(int16_t v) {
    if (v < 0) {
        return (int16_t) -v;
    }
    return v;
}

static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static void wrap_pos(int16_t *v, int16_t max_fp) {
    if (*v < 0) {
        *v = (int16_t) (*v + max_fp);
    } else if (*v >= max_fp) {
        *v = (int16_t) (*v - max_fp);
    }
}

static int16_t wrap_delta(int16_t a, int16_t b, int16_t max_fp) {
    int16_t d = (int16_t) (a - b);
    int16_t half = (int16_t) (max_fp >> 1);
    if (d > half) {
        d = (int16_t) (d - max_fp);
    } else if (d < (int16_t) -half) {
        d = (int16_t) (d + max_fp);
    }
    return d;
}

static uint16_t rng_next(uint16_t *s) {
    uint16_t x = *s;
    x ^= (uint16_t) (x << 7);
    x ^= (uint16_t) (x >> 9);
    x ^= (uint16_t) (x << 8);
    *s = x;
    return x;
}

static int16_t rand_range(struct Game *g, int16_t limit) {
    if (limit <= 0) {
        return 0;
    }
    return (int16_t) (rng_next(&g->rng) % (uint16_t) limit);
}

static int16_t asteroid_radius_fp(uint8_t size) {
    if (size == 3) {
        return (int16_t) (20 * FP_ONE);
    }
    if (size == 2) {
        return (int16_t) (14 * FP_ONE);
    }
    return (int16_t) (9 * FP_ONE);
}

static uint8_t asteroid_angle_index(uint8_t verts, uint8_t i) {
    if (verts == 6) {
        return k_angle_idx_6[i];
    }
    if (verts == 7) {
        return k_angle_idx_7[i];
    }
    return k_angle_idx_8[i];
}

static void reset_ship(struct Game *g) {
    g->ship_x = (int16_t) (g->width_fp >> 1);
    g->ship_y = (int16_t) (g->height_fp >> 1);
    g->ship_vx = 0;
    g->ship_vy = 0;
    g->turn_cd = 0;
    g->friction_cd = 0;
    g->thrusting = 0;
    g->ship_dir = 0;
}

static void spawn_asteroid(struct Game *g, uint8_t size) {
    uint8_t i;
    for (i = 0; i < AST_MAX; ++i) {
        struct Asteroid *a = &g->asteroids[i];
        if (!a->active) {
            int16_t edge = rand_range(g, 4);
            int16_t dir = rand_range(g, DIR_COUNT);
            int16_t speed = (int16_t) (AST_SPEED_BASE + rand_range(g, 6));

            if (edge == 0) {
                a->x = 0;
                a->y = rand_range(g, g->height_fp);
            } else if (edge == 1) {
                a->x = (int16_t) (g->width_fp - 1);
                a->y = rand_range(g, g->height_fp);
            } else if (edge == 2) {
                a->x = rand_range(g, g->width_fp);
                a->y = 0;
            } else {
                a->x = rand_range(g, g->width_fp);
                a->y = (int16_t) (g->height_fp - 1);
            }

            a->vx = (int16_t) ((k_dir_x[dir] * speed) / (DIR_SCALE / 2));
            a->vy = (int16_t) ((k_dir_y[dir] * speed) / (DIR_SCALE / 2));
            if (a->vx == 0 && a->vy == 0) {
                a->vx = 3;
            }
            a->spin = (int8_t) ((int16_t) rand_range(g, 3) - 1);
            a->verts = (uint8_t) (6 + rand_range(g, 3));
            a->angle = (uint8_t) rand_range(g, DIR_COUNT);
            {
                uint8_t vi;
                for (vi = 0; vi < a->verts; ++vi) {
                    a->radii[vi] = (uint8_t) (11 + rand_range(g, 6));
                }
            }
            a->size = size;
            a->active = 1;
            return;
        }
    }
}

static struct Asteroid *find_free_asteroid(struct Game *g) {
    uint8_t i;
    for (i = 0; i < AST_MAX; ++i) {
        if (!g->asteroids[i].active) {
            return &g->asteroids[i];
        }
    }
    return (struct Asteroid *) 0;
}

static void spawn_wave(struct Game *g) {
    int16_t i;
    int16_t count = (int16_t) (AST_START + (g->wave >> 1));
    if (count > 14) {
        count = 14;
    }
    for (i = 0; i < count; ++i) {
        spawn_asteroid(g, 3);
    }
}

static void fire_bullet(struct Game *g) {
    uint8_t i;
    int8_t dx = k_dir_x[g->ship_dir];
    int8_t dy = k_dir_y[g->ship_dir];

    if (g->fire_cd > 0) {
        return;
    }
    for (i = 0; i < BULLET_MAX; ++i) {
        struct Bullet *b = &g->bullets[i];
        if (!b->active) {
            b->x = (int16_t) (g->ship_x + ((dx * SHIP_NOSE * FP_ONE) / DIR_SCALE));
            b->y = (int16_t) (g->ship_y + ((dy * SHIP_NOSE * FP_ONE) / DIR_SCALE));
            b->vx = (int16_t) (g->ship_vx + ((dx * BULLET_SPEED) / 2));
            b->vy = (int16_t) (g->ship_vy + ((dy * BULLET_SPEED) / 2));
            b->life = BULLET_LIFE;
            b->active = 1;
            g->fire_cd = FIRE_COOLDOWN;
            return;
        }
    }
}

static void split_asteroid(struct Game *g, struct Asteroid *a) {
    int16_t vx = a->vx;
    int16_t vy = a->vy;
    uint8_t nsize = (uint8_t) (a->size - 1);

    if (a->size <= 1) {
        return;
    }

    {
        uint8_t k;
        for (k = 0; k < 2; ++k) {
            struct Asteroid *n = find_free_asteroid(g);
            if (n) {
                int16_t tweak = (int16_t) (3 + rand_range(g, 3));
                n->x = a->x;
                n->y = a->y;
                if (k == 0) {
                    n->vx = (int16_t) (vy + tweak);
                    n->vy = (int16_t) (-vx + 1);
                } else {
                    n->vx = (int16_t) (-vy - tweak);
                    n->vy = (int16_t) (vx - 1);
                }
                n->spin = (int8_t) ((int16_t) rand_range(g, 3) - 1);
                if (n->spin == 0) {
                    n->spin = (int8_t) ((k == 0) ? 1 : -1);
                }
                n->verts = (uint8_t) (6 + rand_range(g, 3));
                n->angle = (uint8_t) rand_range(g, DIR_COUNT);
                {
                    uint8_t vi;
                    for (vi = 0; vi < n->verts; ++vi) {
                        n->radii[vi] = (uint8_t) (11 + rand_range(g, 6));
                    }
                }
                n->size = nsize;
                n->active = 1;
            }
        }
    }
}

static void draw_ship(struct Game *g) {
    int16_t sx = (int16_t) (g->ship_x >> FP_SHIFT);
    int16_t sy = (int16_t) (g->ship_y >> FP_SHIFT);
    uint8_t dir = g->ship_dir;
    uint8_t ldir = (uint8_t) ((dir + 10) & DIR_MASK);
    uint8_t rdir = (uint8_t) ((dir + 22) & DIR_MASK);

    int16_t nx = (int16_t) (sx + (k_dir_x[dir] * SHIP_NOSE) / DIR_SCALE);
    int16_t ny = (int16_t) (sy + (k_dir_y[dir] * SHIP_NOSE) / DIR_SCALE);
    int16_t lx = (int16_t) (sx + (k_dir_x[ldir] * SHIP_SIDE) / DIR_SCALE);
    int16_t ly = (int16_t) (sy + (k_dir_y[ldir] * SHIP_SIDE) / DIR_SCALE);
    int16_t rx = (int16_t) (sx + (k_dir_x[rdir] * SHIP_SIDE) / DIR_SCALE);
    int16_t ry = (int16_t) (sy + (k_dir_y[rdir] * SHIP_SIDE) / DIR_SCALE);
    int16_t bx = (int16_t) ((lx + rx) / 2);
    int16_t by = (int16_t) ((ly + ry) / 2);
    int16_t tx = (int16_t) (bx + (k_dir_x[dir] * 5) / DIR_SCALE);
    int16_t ty = (int16_t) (by + (k_dir_y[dir] * 5) / DIR_SCALE);

    platform_draw_line(nx, ny, lx, ly);
    platform_draw_line(lx, ly, tx, ty);
    platform_draw_line(tx, ty, rx, ry);
    platform_draw_line(rx, ry, nx, ny);

    if (g->thrusting) {
        uint8_t bdir = (uint8_t) ((dir + (DIR_COUNT / 2)) & DIR_MASK);
        int16_t fx = (int16_t) (bx + (k_dir_x[bdir] * 9) / DIR_SCALE);
        int16_t fy = (int16_t) (by + (k_dir_y[bdir] * 9) / DIR_SCALE);
        platform_draw_line(lx, ly, fx, fy);
        platform_draw_line(rx, ry, fx, fy);
    }
}

static void draw_bullets(struct Game *g) {
    uint8_t i;
    for (i = 0; i < BULLET_MAX; ++i) {
        struct Bullet *b = &g->bullets[i];
        if (b->active) {
            int16_t x = (int16_t) (b->x >> FP_SHIFT);
            int16_t y = (int16_t) (b->y >> FP_SHIFT);
            platform_draw_line(x - 1, y, x + 1, y);
            platform_draw_line(x, y - 1, x, y + 1);
        }
    }
}

static void draw_asteroid(struct Asteroid *a) {
    uint8_t i;
    int16_t cx = (int16_t) (a->x >> FP_SHIFT);
    int16_t cy = (int16_t) (a->y >> FP_SHIFT);
    int16_t base = 0;
    if (a->size == 3) {
        base = 22;
    } else if (a->size == 2) {
        base = 14;
    } else {
        base = 9;
    }

    for (i = 0; i < a->verts; ++i) {
        uint8_t j = (uint8_t) (i + 1);
        uint8_t di;
        uint8_t dj;
        int16_t ri;
        int16_t rj;
        if (j >= a->verts) {
            j = 0;
        }
        di = (uint8_t) ((a->angle + asteroid_angle_index(a->verts, i)) & DIR_MASK);
        dj = (uint8_t) ((a->angle + asteroid_angle_index(a->verts, j)) & DIR_MASK);
        ri = (int16_t) ((base * a->radii[i]) / 16);
        rj = (int16_t) ((base * a->radii[j]) / 16);
        int16_t x0 = (int16_t) (cx + (k_dir_x[di] * ri) / DIR_SCALE);
        int16_t y0 = (int16_t) (cy + (k_dir_y[di] * ri) / DIR_SCALE);
        int16_t x1 = (int16_t) (cx + (k_dir_x[dj] * rj) / DIR_SCALE);
        int16_t y1 = (int16_t) (cy + (k_dir_y[dj] * rj) / DIR_SCALE);
        platform_draw_line(x0, y0, x1, y1);
    }
}

static void draw_asteroids(struct Game *g) {
    uint8_t i;
    for (i = 0; i < AST_MAX; ++i) {
        if (g->asteroids[i].active) {
            draw_asteroid(&g->asteroids[i]);
        }
    }
}

static void draw_hud(struct Game *g) {
    char score_text[18];
    char wave_text[14];
    int16_t s = g->score;
    int16_t w = g->wave;
    uint8_t pos = 0;

    score_text[pos++] = 'S';
    score_text[pos++] = 'C';
    score_text[pos++] = 'O';
    score_text[pos++] = 'R';
    score_text[pos++] = 'E';
    score_text[pos++] = ':';
    score_text[pos++] = ' ';
    if (s < 0) {
        s = 0;
    }
    if (s >= 10000) {
        score_text[pos++] = (char) ('0' + ((s / 10000) % 10));
    }
    if (s >= 1000) {
        score_text[pos++] = (char) ('0' + ((s / 1000) % 10));
    }
    if (s >= 100) {
        score_text[pos++] = (char) ('0' + ((s / 100) % 10));
    }
    if (s >= 10) {
        score_text[pos++] = (char) ('0' + ((s / 10) % 10));
    }
    score_text[pos++] = (char) ('0' + (s % 10));
    score_text[pos] = '\0';

    pos = 0;
    wave_text[pos++] = 'W';
    wave_text[pos++] = 'A';
    wave_text[pos++] = 'V';
    wave_text[pos++] = 'E';
    wave_text[pos++] = ':';
    wave_text[pos++] = ' ';
    if (w >= 10) {
        wave_text[pos++] = (char) ('0' + ((w / 10) % 10));
    }
    wave_text[pos++] = (char) ('0' + (w % 10));
    wave_text[pos] = '\0';

    platform_draw_text(8, 8, score_text);
    platform_draw_text(8, 22, wave_text);

    {
        char life_text[11];
        uint8_t i;
        life_text[0] = 'L';
        life_text[1] = 'I';
        life_text[2] = 'V';
        life_text[3] = 'E';
        life_text[4] = 'S';
        life_text[5] = ':';
        life_text[6] = ' ';
        for (i = 0; i < g->lives && i < 3; ++i) {
            life_text[7 + i] = '*';
        }
        life_text[7 + g->lives] = '\0';
        platform_draw_text(8, 36, life_text);
    }
}

static void draw_ship_explosion(struct Game *g) {
    uint8_t i;
    int16_t cx;
    int16_t cy;
    int16_t inner;
    int16_t outer;

    if (g->explode_timer <= 0) {
        return;
    }

    cx = (int16_t) (g->explode_x >> FP_SHIFT);
    cy = (int16_t) (g->explode_y >> FP_SHIFT);
    inner = (int16_t) ((12 - g->explode_timer) * 2);
    if (inner < 0) {
        inner = 0;
    }
    outer = (int16_t) (inner + 7);

    for (i = 0; i < DIR_COUNT; i += 4) {
        int16_t x0 = (int16_t) (cx + (k_dir_x[i] * inner) / DIR_SCALE);
        int16_t y0 = (int16_t) (cy + (k_dir_y[i] * inner) / DIR_SCALE);
        int16_t x1 = (int16_t) (cx + (k_dir_x[i] * outer) / DIR_SCALE);
        int16_t y1 = (int16_t) (cy + (k_dir_y[i] * outer) / DIR_SCALE);
        platform_draw_line(x0, y0, x1, y1);
    }

    for (i = 2; i < DIR_COUNT; i += 8) {
        int16_t x0 = (int16_t) (cx + (k_dir_x[i] * (inner / 2)) / DIR_SCALE);
        int16_t y0 = (int16_t) (cy + (k_dir_y[i] * (inner / 2)) / DIR_SCALE);
        int16_t x1 = (int16_t) (cx + (k_dir_x[i] * (outer + 4)) / DIR_SCALE);
        int16_t y1 = (int16_t) (cy + (k_dir_y[i] * (outer + 4)) / DIR_SCALE);
        platform_draw_line(x0, y0, x1, y1);
    }
}

static uint8_t asteroid_count(struct Game *g) {
    uint8_t i;
    uint8_t n = 0;
    for (i = 0; i < AST_MAX; ++i) {
        if (g->asteroids[i].active) {
            ++n;
        }
    }
    return n;
}

static void update_player(struct Game *g, int key) {
    if (g->explode_timer > 0) {
        return;
    }

    g->thrusting = 0;

    if (g->turn_cd > 0) {
        --g->turn_cd;
    }

    if (key == KEY_LEFT) {
        if (g->turn_cd == 0) {
            g->ship_dir = (uint8_t) ((g->ship_dir + DIR_MASK) & DIR_MASK);
            g->turn_cd = SHIP_TURN_DELAY;
        }
    } else if (key == KEY_RIGHT) {
        if (g->turn_cd == 0) {
            g->ship_dir = (uint8_t) ((g->ship_dir + 1) & DIR_MASK);
            g->turn_cd = SHIP_TURN_DELAY;
        }
    } else if (key == KEY_UP || key == KEY_W) {
        int8_t dx = k_dir_x[g->ship_dir];
        int8_t dy = k_dir_y[g->ship_dir];
        g->thrusting = 1;
        g->ship_vx = (int16_t) (g->ship_vx + (dx * SHIP_THRUST) / 8);
        g->ship_vy = (int16_t) (g->ship_vy + (dy * SHIP_THRUST) / 8);
    } else if (key == KEY_SPACE || key == KEY_ENTER) {
        g->fire_queued = 1;
    }

    if (g->fire_queued && g->fire_cd == 0) {
        fire_bullet(g);
        g->fire_queued = 0;
    }

    if (g->thrusting) {
        g->friction_cd = SHIP_FRICTION_DELAY;
    } else {
        if (g->friction_cd > 0) {
            --g->friction_cd;
        } else {
            if (g->ship_vx > 0) {
                g->ship_vx = (int16_t) (g->ship_vx - SHIP_FRICTION);
            } else if (g->ship_vx < 0) {
                g->ship_vx = (int16_t) (g->ship_vx + SHIP_FRICTION);
            }
            if (g->ship_vy > 0) {
                g->ship_vy = (int16_t) (g->ship_vy - SHIP_FRICTION);
            } else if (g->ship_vy < 0) {
                g->ship_vy = (int16_t) (g->ship_vy + SHIP_FRICTION);
            }
            g->friction_cd = SHIP_FRICTION_DELAY;
        }
    }

    g->ship_vx = clamp16(g->ship_vx, -SHIP_MAX_V, SHIP_MAX_V);
    g->ship_vy = clamp16(g->ship_vy, -SHIP_MAX_V, SHIP_MAX_V);
    g->ship_x = (int16_t) (g->ship_x + g->ship_vx);
    g->ship_y = (int16_t) (g->ship_y + g->ship_vy);
    wrap_pos(&g->ship_x, g->width_fp);
    wrap_pos(&g->ship_y, g->height_fp);
}

static void update_bullets(struct Game *g) {
    uint8_t i;
    for (i = 0; i < BULLET_MAX; ++i) {
        struct Bullet *b = &g->bullets[i];
        if (b->active) {
            b->x = (int16_t) (b->x + b->vx);
            b->y = (int16_t) (b->y + b->vy);
            wrap_pos(&b->x, g->width_fp);
            wrap_pos(&b->y, g->height_fp);
            --b->life;
            if (b->life <= 0) {
                b->active = 0;
            }
        }
    }
}

static void update_asteroids(struct Game *g) {
    uint8_t i;
    for (i = 0; i < AST_MAX; ++i) {
        struct Asteroid *a = &g->asteroids[i];
        if (a->active) {
            a->x = (int16_t) (a->x + a->vx);
            a->y = (int16_t) (a->y + a->vy);
            if (g->frame_flip && a->spin != 0) {
                a->angle = (uint8_t) ((a->angle + a->spin + DIR_COUNT) & DIR_MASK);
            }
            wrap_pos(&a->x, g->width_fp);
            wrap_pos(&a->y, g->height_fp);
        }
    }
}

static void check_bullet_hits(struct Game *g) {
    uint8_t bi;
    for (bi = 0; bi < BULLET_MAX; ++bi) {
        struct Bullet *b = &g->bullets[bi];
        if (!b->active) {
            continue;
        }
        {
            uint8_t ai;
            for (ai = 0; ai < AST_MAX; ++ai) {
                struct Asteroid *a = &g->asteroids[ai];
                if (a->active) {
                    int16_t r = asteroid_radius_fp(a->size);
                    int16_t dx = wrap_delta(b->x, a->x, g->width_fp);
                    int16_t dy = wrap_delta(b->y, a->y, g->height_fp);
                    if (iabs16(dx) < r && iabs16(dy) < r) {
                        b->active = 0;
                        a->active = 0;
                        g->score = (int16_t) (g->score + (int16_t) (8 * (4 - a->size)));
                        split_asteroid(g, a);
                        break;
                    }
                }
            }
        }
    }
}

static void check_ship_hits(struct Game *g) {
    uint8_t ai;
    if (g->explode_timer > 0) {
        return;
    }
    for (ai = 0; ai < AST_MAX; ++ai) {
        struct Asteroid *a = &g->asteroids[ai];
        if (a->active) {
            int16_t r = (int16_t) (asteroid_radius_fp(a->size) - (4 * FP_ONE));
            int16_t dx = wrap_delta(g->ship_x, a->x, g->width_fp);
            int16_t dy = wrap_delta(g->ship_y, a->y, g->height_fp);
            if (iabs16(dx) < r && iabs16(dy) < r) {
                g->explode_x = g->ship_x;
                g->explode_y = g->ship_y;
                g->explode_timer = 12;
                if (g->lives > 0) {
                    --g->lives;
                }
                if (g->lives == 0) {
                    g->over = 1;
                }
                reset_ship(g);
                return;
            }
        }
    }
}

static void draw_game_over(struct Game *g) {
    int16_t cx = (int16_t) ((g->width_fp >> FP_SHIFT) >> 1);
    int16_t cy = (int16_t) ((g->height_fp >> FP_SHIFT) >> 1);
    platform_draw_text2x((int16_t) (cx - 80), (int16_t) (cy - 20), "GAME OVER");
    platform_draw_text((int16_t) (cx - 88), (int16_t) (cy + 12), "PRESS ENTER TO RESTART");
    platform_draw_text((int16_t) (cx - 62), (int16_t) (cy + 24), "Q OR ESC TO QUIT");
}

static void game_reset(struct Game *g) {
    uint8_t i;
    g->score = 0;
    g->wave = 1;
    g->lives = 3;
    g->fire_cd = 0;
    g->turn_cd = 0;
    g->friction_cd = 0;
    g->over = 0;
    g->explode_timer = 0;
    g->fire_queued = 0;
    g->frame_flip = 0;
    g->thrusting = 0;
    for (i = 0; i < BULLET_MAX; ++i) {
        g->bullets[i].active = 0;
    }
    for (i = 0; i < AST_MAX; ++i) {
        g->asteroids[i].active = 0;
    }
    reset_ship(g);
    spawn_wave(g);
}

static void frame_fn(int key, void *ctx) {
    struct Game *g = (struct Game *) ctx;

    if (key == KEY_QUIT) {
        g->running = 0;
        return;
    }

    platform_set_color(BACK);
    platform_cls();
    platform_set_color(FORE);

    if (g->over) {
        if (key == KEY_ENTER) {
            game_reset(g);
        }
        draw_hud(g);
        draw_game_over(g);
        return;
    }

    if (g->fire_cd > 0) {
        --g->fire_cd;
    }
    if (g->explode_timer > 0) {
        --g->explode_timer;
    }
    g->frame_flip ^= 1;

    update_player(g, key);
    update_bullets(g);
    update_asteroids(g);
    check_bullet_hits(g);
    check_ship_hits(g);

    if (!g->over && asteroid_count(g) == 0) {
        g->wave = (int16_t) (g->wave + 1);
        spawn_wave(g);
    }

    if (g->explode_timer == 0) {
        draw_ship(g);
    } else {
        draw_ship_explosion(g);
    }
    draw_bullets(g);
    draw_asteroids(g);
    draw_hud(g);
}

static int running_fn(void *ctx) {
    struct Game *g = (struct Game *) ctx;
    return g->running;
}

void game_run(void) {
    struct Game g;

    if (!platform_init()) {
        return;
    }

    g.width_fp = (int16_t) (platform_display_width() << FP_SHIFT);
    g.height_fp = (int16_t) (platform_display_height() << FP_SHIFT);
    g.running = 1;
    g.rng = 0x5AA5u;

    game_reset(&g);
    platform_loop(frame_fn, running_fn, &g, 16);
    platform_shutdown();
}
