#include <partner/conio.h>

#include "game.h"

#include <ugpx.h>
#include <sys/bdos.h>
#include <stdint.h>

#define DIR_COUNT 32
#define DIR_MASK (DIR_COUNT - 1)
#define DIR_RADIUS_MAX 31
#define DIR_SCALE 32
/* DIR_SCALE == 32 == 2^5: use arithmetic right-shift instead of signed
   division so SDCC emits SRA instructions rather than a software div call */
#define DIR_S(x) ((x) >> 5)
#define DRAW_Y_RADIUS(r) (r)

#define SHIP_THRUST 2
#define SHIP_MAX_V 28
#define SHIP_FRICTION 1
#define SHIP_FRICTION_DELAY 3
#define SHIP_NOSE 14
#define SHIP_SIDE 10
#define SHIP_TAIL 2
#define SHIP_TURN_DELAY 0
#define SHIP_DRAW_RADIUS_X SHIP_NOSE
#define SHIP_DRAW_RADIUS_Y SHIP_NOSE

#define BULLET_MAX 10
#define BULLET_LIFE 40
#define BULLET_SPEED 30
#define FIRE_COOLDOWN 7

#define AST_MAX 24
#define AST_TEMPLATE_COUNT 4
#define AST_START 6
#define AST_SPEED_MIN 16
#define AST_SPEED_SPAN 8
#define AST_VEL_SCALE 8

#define KEY_NONE  0
#define KEY_UP    1
#define KEY_DOWN  2
#define KEY_LEFT  3
#define KEY_RIGHT 4
#define KEY_ENTER 5
#define KEY_SPACE 6
#define KEY_W     7
#define KEY_S     8
#define KEY_QUIT  9

#define SCREEN_W 1024
#define SCREEN_H 512
#define AST_MARGIN_X 20
#define AST_MARGIN_Y 20
#define VEL_SCALE 16
#define HUD_X 8
#define HUD_Y 4
#define HUD_W 420
#define HUD_H 20

/* game-over erase rect: covers centred text block */
#define GOVER_X0  340
#define GOVER_Y0  208
#define GOVER_X1  700
#define GOVER_Y1  292
#define TITLE_X0  232
#define TITLE_Y0   48
#define TITLE_X1  792
#define TITLE_Y1  470

struct bullet_rec {
    uint16_t x;
    uint16_t y;
    int8_t dx;
    int8_t dy;
    uint8_t life;
    uint8_t active;
};

struct asteroid_rec {
    uint16_t x;
    uint16_t y;
    int8_t vx;
    int8_t vy;
    uint8_t ex;
    uint8_t ey;
    uint8_t size;
    uint8_t shape;
    uint8_t angle;
    uint8_t active;
};

struct game_state {
    uint16_t ship_x;
    uint16_t ship_y;
    int8_t ship_vx;
    int8_t ship_vy;
    uint8_t ship_ex;
    uint8_t ship_ey;
    uint8_t fire_cd;
    uint8_t turn_cd;
    uint8_t friction_cd;
    uint16_t explode_x;
    uint16_t explode_y;
    uint8_t explode_timer;
    uint16_t score;
    uint8_t wave;
    uint8_t ship_dir;
    uint8_t lives;
    uint8_t thrusting;
    uint8_t fire_queued;
    uint8_t running;
    uint8_t over;
    uint8_t started;
    uint8_t bullet_count;
    uint8_t ast_count;
    uint8_t active_asteroids;
    uint16_t rng;
    uint8_t bullet_idx[BULLET_MAX];
    uint8_t ast_idx[AST_MAX];
    struct bullet_rec bullets[BULLET_MAX];
    struct asteroid_rec asteroids[AST_MAX];
};

/* compact per-page draw records (what was drawn last time on this page) */
struct bul_rec {
    int16_t x, y;
};

struct ast_rec {
    int16_t cx, cy;
    uint8_t angle;
    uint8_t size;
    uint8_t shape;
};

struct asteroid_template {
    uint8_t verts;
    uint8_t radii[8];
};

struct cached_path {
    int8_t start_x;
    int8_t start_y;
    int8_t dx[8];
    int8_t dy[8];
};

struct line_span {
    uint16_t x0;
    uint16_t x1;
};

struct ship_shape {
    int8_t start_x;
    int8_t start_y;
    int8_t dx[4];
    int8_t dy[4];
    int8_t thrust_lx;
    int8_t thrust_ly;
    int8_t thrust_ldx;
    int8_t thrust_ldy;
    int8_t thrust_rx;
    int8_t thrust_ry;
    int8_t thrust_rdx;
    int8_t thrust_rdy;
};

struct page_rec {
    uint8_t valid;           /* 0 on first use: nothing to erase yet */
    uint8_t ship_visible;
    int16_t ship_sx, ship_sy;
    uint8_t ship_dir, ship_thrusting;
    uint8_t exp_active;
    int16_t exp_cx, exp_cy, exp_inner, exp_outer;
    uint8_t game_over_shown;
    uint8_t title_shown;
    uint16_t hud_score;
    uint8_t hud_wave;
    uint8_t hud_lives;
    uint8_t bul_count;
    uint8_t ast_count;
    struct bul_rec bul[BULLET_MAX];
    struct ast_rec ast[AST_MAX];
};

static const int8_t k_dir_x[DIR_COUNT] = {
    32, 31, 30, 27, 23, 18, 12, 6, 0, -6, -12, -18, -23, -27, -30, -31,
    -32, -31, -30, -27, -23, -18, -12, -6, 0, 6, 12, 18, 23, 27, 30, 31
};
static const int8_t k_dir_y[DIR_COUNT] = {
    0, -6, -12, -18, -23, -27, -30, -31, -32, -31, -30, -27, -23, -18, -12, -6,
    0, 6, 12, 18, 23, 27, 30, 31, 32, 31, 30, 27, 23, 18, 12, 6
};
#include "kamenje_logo.inc"
static const uint8_t k_angle_idx_6[6] = {0, 6, 10, 16, 22, 26};
static const uint8_t k_angle_idx_7[7] = {0, 4, 10, 14, 18, 24, 28};
static const uint8_t k_angle_idx_8[8] = {0, 4, 8, 12, 16, 20, 24, 28};
static const struct asteroid_template k_ast_template[AST_TEMPLATE_COUNT] = {
    {4, {16, 12, 15, 11, 0, 0, 0, 0}},
    {5, {15, 12, 16, 13, 11, 0, 0, 0}},
    {6, {14, 16, 12, 15, 11, 13, 0, 0}},
    {5, {13, 16, 11, 15, 12, 0, 0, 0}}
};
static const uint8_t k_ast_draw_radius_x[4] = {0, 9, 14, 22};
static const uint8_t k_ast_draw_radius_y[4] = {0, 10, 14, 22};
static const uint8_t k_ast_hit_radius[4] = {0, 9, 14, 20};
static const uint8_t k_ast_ship_hit_radius[4] = {0, 5, 10, 16};
static const uint16_t k_ast_left_x[4] = {0, 29, 34, 42};
static const uint16_t k_ast_right_x[4] = {0, 994, 989, 981};
static const uint16_t k_ast_top_y[4] = {0, 30, 34, 42};
static const uint16_t k_ast_bottom_y[4] = {0, 481, 477, 469};
static const uint16_t k_ast_span_x[4] = {0, 966, 956, 940};
static const uint16_t k_ast_span_y[4] = {0, 451, 443, 427};
static const uint8_t k_ast_radius_map[4][6] = {
    {0, 0, 0, 0, 0, 0},
    {6, 6, 7, 7, 8, 9},
    {9, 10, 11, 12, 13, 14},
    {15, 16, 17, 19, 20, 22}
};
static const int8_t k_dir_off_x[32][DIR_COUNT] = {
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    { 1,  0,  0,  0,  0,  0,  0,  0,  0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0},
    { 2,  1,  1,  1,  1,  1,  0,  0,  0, -1, -1, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1,  0,  0,  0,  1,  1,  1,  1,  1},
    { 3,  2,  2,  2,  2,  1,  1,  0,  0, -1, -2, -2, -3, -3, -3, -3, -3, -3, -3, -3, -3, -2, -2, -1,  0,  0,  1,  1,  2,  2,  2,  2},
    { 4,  3,  3,  3,  2,  2,  1,  0,  0, -1, -2, -3, -3, -4, -4, -4, -4, -4, -4, -4, -3, -3, -2, -1,  0,  0,  1,  2,  2,  3,  3,  3},
    { 5,  4,  4,  4,  3,  2,  1,  0,  0, -1, -2, -3, -4, -5, -5, -5, -5, -5, -5, -5, -4, -3, -2, -1,  0,  0,  1,  2,  3,  4,  4,  4},
    { 6,  5,  5,  5,  4,  3,  2,  1,  0, -2, -3, -4, -5, -6, -6, -6, -6, -6, -6, -6, -5, -4, -3, -2,  0,  1,  2,  3,  4,  5,  5,  5},
    { 7,  6,  6,  5,  5,  3,  2,  1,  0, -2, -3, -4, -6, -6, -7, -7, -7, -7, -7, -6, -6, -4, -3, -2,  0,  1,  2,  3,  5,  5,  6,  6},
    { 8,  7,  7,  6,  5,  4,  3,  1,  0, -2, -3, -5, -6, -7, -8, -8, -8, -8, -8, -7, -6, -5, -3, -2,  0,  1,  3,  4,  5,  6,  7,  7},
    { 9,  8,  8,  7,  6,  5,  3,  1,  0, -2, -4, -6, -7, -8, -9, -9, -9, -9, -9, -8, -7, -6, -4, -2,  0,  1,  3,  5,  6,  7,  8,  8},
    {10,  9,  9,  8,  7,  5,  3,  1,  0, -2, -4, -6, -8, -9, -10, -10, -10, -10, -10, -9, -8, -6, -4, -2,  0,  1,  3,  5,  7,  8,  9,  9},
    {11, 10, 10,  9,  7,  6,  4,  2,  0, -3, -5, -7, -8, -10, -11, -11, -11, -11, -11, -10, -8, -7, -5, -3,  0,  2,  4,  6,  7,  9, 10, 10},
    {12, 11, 11, 10,  8,  6,  4,  2,  0, -3, -5, -7, -9, -11, -12, -12, -12, -12, -12, -11, -9, -7, -5, -3,  0,  2,  4,  6,  8, 10, 11, 11},
    {13, 12, 12, 10,  9,  7,  4,  2,  0, -3, -5, -8, -10, -11, -13, -13, -13, -13, -13, -11, -10, -8, -5, -3,  0,  2,  4,  7,  9, 10, 12, 12},
    {14, 13, 13, 11, 10,  7,  5,  2,  0, -3, -6, -8, -11, -12, -14, -14, -14, -14, -14, -12, -11, -8, -6, -3,  0,  2,  5,  7, 10, 11, 13, 13},
    {15, 14, 14, 12, 10,  8,  5,  2,  0, -3, -6, -9, -11, -13, -15, -15, -15, -15, -15, -13, -11, -9, -6, -3,  0,  2,  5,  8, 10, 12, 14, 14},
    {16, 15, 15, 13, 11,  9,  6,  3,  0, -3, -6, -9, -12, -14, -15, -16, -16, -16, -15, -14, -12, -9, -6, -3,  0,  3,  6,  9, 11, 13, 15, 15},
    {17, 16, 15, 14, 12,  9,  6,  3,  0, -4, -7, -10, -13, -15, -16, -17, -17, -17, -16, -15, -13, -10, -7, -4,  0,  3,  6,  9, 12, 14, 15, 16},
    {18, 17, 16, 15, 12, 10,  6,  3,  0, -4, -7, -11, -13, -16, -17, -18, -18, -18, -17, -16, -13, -11, -7, -4,  0,  3,  6, 10, 12, 15, 16, 17},
    {19, 18, 17, 16, 13, 10,  7,  3,  0, -4, -8, -11, -14, -17, -18, -19, -19, -19, -18, -17, -14, -11, -8, -4,  0,  3,  7, 10, 13, 16, 17, 18},
    {20, 19, 18, 16, 14, 11,  7,  3,  0, -4, -8, -12, -15, -17, -19, -20, -20, -20, -19, -17, -15, -12, -8, -4,  0,  3,  7, 11, 14, 16, 18, 19},
    {21, 20, 19, 17, 15, 11,  7,  3,  0, -4, -8, -12, -16, -18, -20, -21, -21, -21, -20, -18, -16, -12, -8, -4,  0,  3,  7, 11, 15, 17, 19, 20},
    {22, 21, 20, 18, 15, 12,  8,  4,  0, -5, -9, -13, -16, -19, -21, -22, -22, -22, -21, -19, -16, -13, -9, -5,  0,  4,  8, 12, 15, 18, 20, 21},
    {23, 22, 21, 19, 16, 12,  8,  4,  0, -5, -9, -13, -17, -20, -22, -23, -23, -23, -22, -20, -17, -13, -9, -5,  0,  4,  8, 12, 16, 19, 21, 22},
    {24, 23, 22, 20, 17, 13,  9,  4,  0, -5, -9, -14, -18, -21, -23, -24, -24, -24, -23, -21, -18, -14, -9, -5,  0,  4,  9, 13, 17, 20, 22, 23},
    {25, 24, 23, 21, 17, 14,  9,  4,  0, -5, -10, -15, -18, -22, -24, -25, -25, -25, -24, -22, -18, -15, -10, -5,  0,  4,  9, 14, 17, 21, 23, 24},
    {26, 25, 24, 21, 18, 14,  9,  4,  0, -5, -10, -15, -19, -22, -25, -26, -26, -26, -25, -22, -19, -15, -10, -5,  0,  4,  9, 14, 18, 21, 24, 25},
    {27, 26, 25, 22, 19, 15, 10,  5,  0, -6, -11, -16, -20, -23, -26, -27, -27, -27, -26, -23, -20, -16, -11, -6,  0,  5, 10, 15, 19, 22, 25, 26},
    {28, 27, 26, 23, 20, 15, 10,  5,  0, -6, -11, -16, -21, -24, -27, -28, -28, -28, -27, -24, -21, -16, -11, -6,  0,  5, 10, 15, 20, 23, 26, 27},
    {29, 28, 27, 24, 20, 16, 10,  5,  0, -6, -11, -17, -21, -25, -28, -29, -29, -29, -28, -25, -21, -17, -11, -6,  0,  5, 10, 16, 20, 24, 27, 28},
    {30, 29, 28, 25, 21, 16, 11,  5,  0, -6, -12, -17, -22, -26, -29, -30, -30, -30, -29, -26, -22, -17, -12, -6,  0,  5, 11, 16, 21, 25, 28, 29},
    {31, 30, 29, 26, 22, 17, 11,  5,  0, -6, -12, -18, -23, -27, -30, -31, -31, -31, -30, -27, -23, -18, -12, -6,  0,  5, 11, 17, 22, 26, 29, 30},
};
static const int8_t k_dir_off_y[32][DIR_COUNT] = {
    { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    { 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  0,  0,  0,  0,  0,  0,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0},
    { 0, -1, -1, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -1, -1,  0,  0,  0,  1,  1,  1,  1,  1,  2,  1,  1,  1,  1,  1,  0,  0},
    { 0, -1, -2, -2, -3, -3, -3, -3, -3, -3, -3, -3, -3, -2, -2, -1,  0,  0,  1,  1,  2,  2,  2,  2,  3,  2,  2,  2,  2,  1,  1,  0},
    { 0, -1, -2, -3, -3, -4, -4, -4, -4, -4, -4, -4, -3, -3, -2, -1,  0,  0,  1,  2,  2,  3,  3,  3,  4,  3,  3,  3,  2,  2,  1,  0},
    { 0, -1, -2, -3, -4, -5, -5, -5, -5, -5, -5, -5, -4, -3, -2, -1,  0,  0,  1,  2,  3,  4,  4,  4,  5,  4,  4,  4,  3,  2,  1,  0},
    { 0, -2, -3, -4, -5, -6, -6, -6, -6, -6, -6, -6, -5, -4, -3, -2,  0,  1,  2,  3,  4,  5,  5,  5,  6,  5,  5,  5,  4,  3,  2,  1},
    { 0, -2, -3, -4, -6, -6, -7, -7, -7, -7, -7, -6, -6, -4, -3, -2,  0,  1,  2,  3,  5,  5,  6,  6,  7,  6,  6,  5,  5,  3,  2,  1},
    { 0, -2, -3, -5, -6, -7, -8, -8, -8, -8, -8, -7, -6, -5, -3, -2,  0,  1,  3,  4,  5,  6,  7,  7,  8,  7,  7,  6,  5,  4,  3,  1},
    { 0, -2, -4, -6, -7, -8, -9, -9, -9, -9, -9, -8, -7, -6, -4, -2,  0,  1,  3,  5,  6,  7,  8,  8,  9,  8,  8,  7,  6,  5,  3,  1},
    { 0, -2, -4, -6, -8, -9, -10, -10, -10, -10, -10, -9, -8, -6, -4, -2,  0,  1,  3,  5,  7,  8,  9,  9, 10,  9,  9,  8,  7,  5,  3,  1},
    { 0, -3, -5, -7, -8, -10, -11, -11, -11, -11, -11, -10, -8, -7, -5, -3,  0,  2,  4,  6,  7,  9, 10, 10, 11, 10, 10,  9,  7,  6,  4,  2},
    { 0, -3, -5, -7, -9, -11, -12, -12, -12, -12, -12, -11, -9, -7, -5, -3,  0,  2,  4,  6,  8, 10, 11, 11, 12, 11, 11, 10,  8,  6,  4,  2},
    { 0, -3, -5, -8, -10, -11, -13, -13, -13, -13, -13, -11, -10, -8, -5, -3,  0,  2,  4,  7,  9, 10, 12, 12, 13, 12, 12, 10,  9,  7,  4,  2},
    { 0, -3, -6, -8, -11, -12, -14, -14, -14, -14, -14, -12, -11, -8, -6, -3,  0,  2,  5,  7, 10, 11, 13, 13, 14, 13, 13, 11, 10,  7,  5,  2},
    { 0, -3, -6, -9, -11, -13, -15, -15, -15, -15, -15, -13, -11, -9, -6, -3,  0,  2,  5,  8, 10, 12, 14, 14, 15, 14, 14, 12, 10,  8,  5,  2},
    { 0, -3, -6, -9, -12, -14, -15, -16, -16, -16, -15, -14, -12, -9, -6, -3,  0,  3,  6,  9, 11, 13, 15, 15, 16, 15, 15, 13, 11,  9,  6,  3},
    { 0, -4, -7, -10, -13, -15, -16, -17, -17, -17, -16, -15, -13, -10, -7, -4,  0,  3,  6,  9, 12, 14, 15, 16, 17, 16, 15, 14, 12,  9,  6,  3},
    { 0, -4, -7, -11, -13, -16, -17, -18, -18, -18, -17, -16, -13, -11, -7, -4,  0,  3,  6, 10, 12, 15, 16, 17, 18, 17, 16, 15, 12, 10,  6,  3},
    { 0, -4, -8, -11, -14, -17, -18, -19, -19, -19, -18, -17, -14, -11, -8, -4,  0,  3,  7, 10, 13, 16, 17, 18, 19, 18, 17, 16, 13, 10,  7,  3},
    { 0, -4, -8, -12, -15, -17, -19, -20, -20, -20, -19, -17, -15, -12, -8, -4,  0,  3,  7, 11, 14, 16, 18, 19, 20, 19, 18, 16, 14, 11,  7,  3},
    { 0, -4, -8, -12, -16, -18, -20, -21, -21, -21, -20, -18, -16, -12, -8, -4,  0,  3,  7, 11, 15, 17, 19, 20, 21, 20, 19, 17, 15, 11,  7,  3},
    { 0, -5, -9, -13, -16, -19, -21, -22, -22, -22, -21, -19, -16, -13, -9, -5,  0,  4,  8, 12, 15, 18, 20, 21, 22, 21, 20, 18, 15, 12,  8,  4},
    { 0, -5, -9, -13, -17, -20, -22, -23, -23, -23, -22, -20, -17, -13, -9, -5,  0,  4,  8, 12, 16, 19, 21, 22, 23, 22, 21, 19, 16, 12,  8,  4},
    { 0, -5, -9, -14, -18, -21, -23, -24, -24, -24, -23, -21, -18, -14, -9, -5,  0,  4,  9, 13, 17, 20, 22, 23, 24, 23, 22, 20, 17, 13,  9,  4},
    { 0, -5, -10, -15, -18, -22, -24, -25, -25, -25, -24, -22, -18, -15, -10, -5,  0,  4,  9, 14, 17, 21, 23, 24, 25, 24, 23, 21, 17, 14,  9,  4},
    { 0, -5, -10, -15, -19, -22, -25, -26, -26, -26, -25, -22, -19, -15, -10, -5,  0,  4,  9, 14, 18, 21, 24, 25, 26, 25, 24, 21, 18, 14,  9,  4},
    { 0, -6, -11, -16, -20, -23, -26, -27, -27, -27, -26, -23, -20, -16, -11, -6,  0,  5, 10, 15, 19, 22, 25, 26, 27, 26, 25, 22, 19, 15, 10,  5},
    { 0, -6, -11, -16, -21, -24, -27, -28, -28, -28, -27, -24, -21, -16, -11, -6,  0,  5, 10, 15, 20, 23, 26, 27, 28, 27, 26, 23, 20, 15, 10,  5},
    { 0, -6, -11, -17, -21, -25, -28, -29, -29, -29, -28, -25, -21, -17, -11, -6,  0,  5, 10, 16, 20, 24, 27, 28, 29, 28, 27, 24, 20, 16, 10,  5},
    { 0, -6, -12, -17, -22, -26, -29, -30, -30, -30, -29, -26, -22, -17, -12, -6,  0,  5, 11, 16, 21, 25, 28, 29, 30, 29, 28, 25, 21, 16, 11,  5},
    { 0, -6, -12, -18, -23, -27, -30, -31, -31, -31, -30, -27, -23, -18, -12, -6,  0,  5, 11, 17, 22, 26, 29, 30, 31, 30, 29, 26, 22, 17, 11,  5},
};

/* font from idp8x16_font.s */
extern void idp8x16_font;

/* global game state: too large for Z80 stack */
static struct game_state g;
static struct ship_shape k_ship_shape[DIR_COUNT];
static struct cached_path k_ast_shape[4][AST_TEMPLATE_COUNT][DIR_COUNT];

/* double buffer pages */
static uint8_t display_pg;
static uint8_t write_pg;

/* per-page draw records */
static struct page_rec pg_rec[2];

static void clear_text_screen(void) {
    clrscr();
}

static void hide_text_cursor(void) {
    setcursortype(NOCURSOR);
}

static void show_text_cursor(void) {
    setcursortype(NORMALCURSOR);
}

/* ---------- helpers ---------- */

static int8_t clamp8(int8_t v, int8_t lo, int8_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint16_t wrap_x_step(uint16_t x, int8_t step) {
    return (uint16_t)((x + SCREEN_W + (int16_t)step) & (SCREEN_W - 1));
}

static uint16_t wrap_y_step(uint16_t y, int8_t step) {
    return (uint16_t)((y + SCREEN_H + (int16_t)step) & (SCREEN_H - 1));
}

static uint16_t move_bres_x(uint16_t x, int8_t v, uint8_t *err) {
    uint8_t mag;
    if (v == 0) return x;
    mag = (uint8_t)((v < 0) ? -v : v);
    *err = (uint8_t)(*err + mag);
    while (*err >= VEL_SCALE) {
        x = wrap_x_step(x, (int8_t)((v < 0) ? -1 : 1));
        *err = (uint8_t)(*err - VEL_SCALE);
    }
    return x;
}

static uint16_t move_bres_y(uint16_t y, int8_t v, uint8_t *err) {
    uint8_t mag;
    if (v == 0) return y;
    mag = (uint8_t)((v < 0) ? -v : v);
    *err = (uint8_t)(*err + mag);
    while (*err >= VEL_SCALE) {
        y = wrap_y_step(y, (int8_t)((v < 0) ? -1 : 1));
        *err = (uint8_t)(*err - VEL_SCALE);
    }
    return y;
}

static uint16_t move_bres_fast_x(uint16_t x, int8_t v, uint8_t *err) {
    uint8_t mag;
    if (v == 0) return x;
    mag = (uint8_t)((v < 0) ? -v : v);
    *err = (uint8_t)(*err + mag);
    while (*err >= AST_VEL_SCALE) {
        x = wrap_x_step(x, (int8_t)((v < 0) ? -1 : 1));
        *err = (uint8_t)(*err - AST_VEL_SCALE);
    }
    return x;
}

static uint16_t move_bres_fast_y(uint16_t y, int8_t v, uint8_t *err) {
    uint8_t mag;
    if (v == 0) return y;
    mag = (uint8_t)((v < 0) ? -v : v);
    *err = (uint8_t)(*err + mag);
    while (*err >= AST_VEL_SCALE) {
        y = wrap_y_step(y, (int8_t)((v < 0) ? -1 : 1));
        *err = (uint8_t)(*err - AST_VEL_SCALE);
    }
    return y;
}

static uint16_t wrap_inset_x(uint16_t x, uint8_t size) {
    if (x < k_ast_left_x[size]) {
        return (uint16_t)(x + k_ast_span_x[size]);
    }
    if (x > k_ast_right_x[size]) {
        return (uint16_t)(x - k_ast_span_x[size]);
    }
    return x;
}

static uint16_t wrap_inset_y(uint16_t y, uint8_t size) {
    if (y < k_ast_top_y[size]) {
        return (uint16_t)(y + k_ast_span_y[size]);
    }
    if (y > k_ast_bottom_y[size]) {
        return (uint16_t)(y - k_ast_span_y[size]);
    }
    return y;
}

static uint16_t wrap_abs_dx(uint16_t a, uint16_t b) {
    uint16_t d = (uint16_t)((a - b) & (SCREEN_W - 1));
    if (d > (SCREEN_W >> 1)) {
        d = (uint16_t)(SCREEN_W - d);
    }
    return d;
}

static uint16_t wrap_abs_dy(uint16_t a, uint16_t b) {
    uint16_t d = (uint16_t)((a - b) & (SCREEN_H - 1));
    if (d > (SCREEN_H >> 1)) {
        d = (uint16_t)(SCREEN_H - d);
    }
    return d;
}

static uint16_t rng_next(uint16_t *s) {
    uint16_t x = *s;
    x ^= (uint16_t)(x << 7);
    x ^= (uint16_t)(x >> 9);
    x ^= (uint16_t)(x << 8);
    *s = x;
    return x;
}

static uint8_t rand_mod3(struct game_state *g) {
    uint8_t r = (uint8_t)(rng_next(&g->rng) >> 8);
    while (r >= 252) r = (uint8_t)(rng_next(&g->rng) >> 8);
    while (r >= 3) r = (uint8_t)(r - 3);
    return r;
}

static uint8_t rand_mod6(struct game_state *g) {
    uint8_t r = (uint8_t)(rng_next(&g->rng) >> 8);
    while (r >= 252) r = (uint8_t)(rng_next(&g->rng) >> 8);
    while (r >= 6) r = (uint8_t)(r - 6);
    return r;
}

static uint16_t rand_mask(struct game_state *g, uint16_t mask) {
    return (uint16_t)(rng_next(&g->rng) & mask);
}

static uint16_t rand_inset_y(struct game_state *g, uint16_t min_y, uint16_t max_y) {
    uint16_t y;
    do {
        y = rand_mask(g, SCREEN_H - 1);
    } while (y < min_y || y > max_y);
    return y;
}

static int16_t rand_inset_x(struct game_state *g, uint16_t min_x, uint16_t max_x) {
    uint16_t x;
    do {
        x = rand_mask(g, SCREEN_W - 1);
    } while (x < min_x || x > max_x);
    return (int16_t)x;
}

static uint8_t asteroid_angle_index(uint8_t verts, uint8_t i) {
    if (verts == 6) return k_angle_idx_6[i];
    if (verts == 7) return k_angle_idx_7[i];
    return k_angle_idx_8[i];
}

static void init_ship_shape_cache(void) {
    uint8_t dir;
    for (dir = 0; dir < DIR_COUNT; ++dir) {
        uint8_t ldir = (uint8_t)((dir + 10) & DIR_MASK);
        uint8_t rdir = (uint8_t)((dir + 22) & DIR_MASK);
        uint8_t nose_y = DRAW_Y_RADIUS(SHIP_NOSE);
        uint8_t side_y = DRAW_Y_RADIUS(SHIP_SIDE);
        uint8_t back_dir = (uint8_t)((dir + (DIR_COUNT / 2)) & DIR_MASK);
        int8_t nx = k_dir_off_x[SHIP_NOSE][dir];
        int8_t ny = k_dir_off_y[nose_y][dir];
        int8_t lx = k_dir_off_x[SHIP_SIDE][ldir];
        int8_t ly = k_dir_off_y[side_y][ldir];
        int8_t rx = k_dir_off_x[SHIP_SIDE][rdir];
        int8_t ry = k_dir_off_y[side_y][rdir];
        int8_t bx = (int8_t)((lx + rx) >> 1);
        int8_t by = (int8_t)((ly + ry) >> 1);
        int8_t tx = (int8_t)(bx + k_dir_off_x[SHIP_TAIL][dir]);
        int8_t ty = (int8_t)(by + k_dir_off_y[DRAW_Y_RADIUS(SHIP_TAIL)][dir]);
        int8_t fx = (int8_t)(bx + k_dir_off_x[9][back_dir]);
        int8_t fy = (int8_t)(by + k_dir_off_y[DRAW_Y_RADIUS(9)][back_dir]);
        struct ship_shape *s = &k_ship_shape[dir];

        s->start_x = nx;
        s->start_y = ny;
        s->dx[0] = (int8_t)(lx - nx);
        s->dy[0] = (int8_t)(ly - ny);
        s->dx[1] = (int8_t)(tx - lx);
        s->dy[1] = (int8_t)(ty - ly);
        s->dx[2] = (int8_t)(rx - tx);
        s->dy[2] = (int8_t)(ry - ty);
        s->dx[3] = (int8_t)(nx - rx);
        s->dy[3] = (int8_t)(ny - ry);
        s->thrust_lx = lx;
        s->thrust_ly = ly;
        s->thrust_ldx = (int8_t)(fx - lx);
        s->thrust_ldy = (int8_t)(fy - ly);
        s->thrust_rx = rx;
        s->thrust_ry = ry;
        s->thrust_rdx = (int8_t)(fx - rx);
        s->thrust_rdy = (int8_t)(fy - ry);
    }
}

static void init_asteroid_shape_cache(void) {
    uint8_t size;
    for (size = 1; size <= 3; ++size) {
        uint8_t shape;
        for (shape = 0; shape < AST_TEMPLATE_COUNT; ++shape) {
            const struct asteroid_template *tpl = &k_ast_template[shape];
            uint8_t dir;
            for (dir = 0; dir < DIR_COUNT; ++dir) {
                struct cached_path *path = &k_ast_shape[size][shape][dir];
                uint8_t verts = tpl->verts;
                uint8_t vi;
                uint8_t dj = (uint8_t)((dir + asteroid_angle_index(verts, 0)) & DIR_MASK);
                uint8_t radius = k_ast_radius_map[size][tpl->radii[0] - 11];
                uint8_t radius_y = DRAW_Y_RADIUS(radius);
                int8_t px = k_dir_off_x[radius][dj];
                int8_t py = k_dir_off_y[radius_y][dj];

                path->start_x = px;
                path->start_y = py;
                for (vi = 1; vi <= verts; ++vi) {
                    uint8_t next_vi = vi;
                    int8_t x1, y1;
                    if (next_vi >= verts) next_vi = 0;
                    dj = (uint8_t)((dir + asteroid_angle_index(verts, next_vi)) & DIR_MASK);
                    radius = k_ast_radius_map[size][tpl->radii[next_vi] - 11];
                    radius_y = DRAW_Y_RADIUS(radius);
                    x1 = k_dir_off_x[radius][dj];
                    y1 = k_dir_off_y[radius_y][dj];
                    path->dx[vi - 1] = (int8_t)(x1 - px);
                    path->dy[vi - 1] = (int8_t)(y1 - py);
                    px = x1;
                    py = y1;
                }
            }
        }
    }
}

static void init_shape_cache(void) {
    init_ship_shape_cache();
    init_asteroid_shape_cache();
}

static void append_dec_place(char *buf, uint8_t *pos, uint16_t *value,
                             uint16_t place, uint8_t *started) {
    uint8_t digit = 0;
    while (*value >= place) {
        *value = (uint16_t)(*value - place);
        ++digit;
    }
    if (*started || digit || place == 1) {
        buf[(*pos)++] = (char)('0' + digit);
        *started = 1;
    }
}

static uint8_t append_u16(char *buf, uint8_t pos, uint16_t value) {
    uint8_t started = 0;
    append_dec_place(buf, &pos, &value, 10000, &started);
    append_dec_place(buf, &pos, &value, 1000, &started);
    append_dec_place(buf, &pos, &value, 100, &started);
    append_dec_place(buf, &pos, &value, 10, &started);
    append_dec_place(buf, &pos, &value, 1, &started);
    return pos;
}

/* ---------- input ---------- */

static int read_key(void) {
    uint8_t c = bdos(C_RAWIO, 0xFF);
    if (c == 'a' || c == 'A') return KEY_LEFT;
    if (c == 'd' || c == 'D') return KEY_RIGHT;
    if (c == 'w' || c == 'W') return KEY_W;
    if (c == ' ')              return KEY_SPACE;
    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == 'q' || c == 'Q' || c == 27) return KEY_QUIT;
    return KEY_NONE;
}

/* ---------- render primitives (caller sets color before calling) ---------- */

static void render_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    gxy((coord)x0, (coord)y0);
    gdrawd((coord)(x1 - x0), (coord)(y1 - y0));
}

static uint8_t fully_visible(int16_t x, int16_t y, int16_t rx, int16_t ry) {
    if (x < rx || x >= (int16_t)(SCREEN_W - rx)) return 0;
    if (y < ry || y >= (int16_t)(SCREEN_H - ry)) return 0;
    return 1;
}

static uint8_t clamp_radius(int16_t r) {
    if (r < 0) return 0;
    if (r > DIR_RADIUS_MAX) return DIR_RADIUS_MAX;
    return (uint8_t)r;
}

static uint8_t render_ship(int16_t sx, int16_t sy, uint8_t dir, uint8_t thrusting) {
    const struct ship_shape *s = &k_ship_shape[dir];
    if (!fully_visible(sx, sy, SHIP_DRAW_RADIUS_X, SHIP_DRAW_RADIUS_Y))
        return 0;
    gxy((coord)(sx + s->start_x), (coord)(sy + s->start_y));
    gdrawd((coord)s->dx[0], (coord)s->dy[0]);
    gdrawd((coord)s->dx[1], (coord)s->dy[1]);
    gdrawd((coord)s->dx[2], (coord)s->dy[2]);
    gdrawd((coord)s->dx[3], (coord)s->dy[3]);
    if (thrusting) {
        gxy((coord)(sx + s->thrust_lx), (coord)(sy + s->thrust_ly));
        gdrawd((coord)s->thrust_ldx, (coord)s->thrust_ldy);
        gxy((coord)(sx + s->thrust_rx), (coord)(sy + s->thrust_ry));
        gdrawd((coord)s->thrust_rdx, (coord)s->thrust_rdy);
    }
    return 1;
}

static void render_bullet(int16_t x, int16_t y) {
    gxy((coord)x, (coord)y);
    gdrawd(0, 0);
}

static void render_asteroid(int16_t cx, int16_t cy, uint8_t size,
                            uint8_t shape, uint8_t angle) {
    const struct asteroid_template *tpl = &k_ast_template[shape];
    const struct cached_path *path = &k_ast_shape[size][shape][angle];
    uint8_t vi;
    gxy((coord)(cx + path->start_x), (coord)(cy + path->start_y));
    for (vi = 0; vi < tpl->verts; ++vi) {
        gdrawd((coord)path->dx[vi], (coord)path->dy[vi]);
    }
}

static uint8_t render_explosion(int16_t cx, int16_t cy, int16_t inner, int16_t outer) {
    uint8_t i;
    uint8_t inner_r = clamp_radius(inner);
    uint8_t outer_r = clamp_radius(outer);
    uint8_t inner_y = DRAW_Y_RADIUS(inner_r);
    uint8_t outer_y = DRAW_Y_RADIUS(outer_r);
    if (!fully_visible(cx, cy, outer, outer))
        return 0;
    for (i = 0; i < DIR_COUNT; i += 4) {
        int8_t x0 = k_dir_off_x[inner_r][i];
        int8_t y0 = k_dir_off_y[inner_y][i];
        int8_t x1 = k_dir_off_x[outer_r][i];
        int8_t y1 = k_dir_off_y[outer_y][i];
        gxy((coord)(cx + x0), (coord)(cy + y0));
        gdrawd((coord)(x1 - x0), (coord)(y1 - y0));
    }
    for (i = 2; i < DIR_COUNT; i += 8) {
        uint8_t half_inner = clamp_radius((int16_t)(inner_r >> 1));
        uint8_t long_outer = clamp_radius((int16_t)(outer_r + 4));
        uint8_t half_inner_y = DRAW_Y_RADIUS(half_inner);
        uint8_t long_outer_y = DRAW_Y_RADIUS(long_outer);
        int8_t x0 = k_dir_off_x[half_inner][i];
        int8_t y0 = k_dir_off_y[half_inner_y][i];
        int8_t x1 = k_dir_off_x[long_outer][i];
        int8_t y1 = k_dir_off_y[long_outer_y][i];
        gxy((coord)(cx + x0), (coord)(cy + y0));
        gdrawd((coord)(x1 - x0), (coord)(y1 - y0));
    }
    return 1;
}

/* ---------- game logic ---------- */

static void reset_ship(struct game_state *g) {
    g->ship_x = (uint16_t)(SCREEN_W >> 1);
    g->ship_y = (uint16_t)(SCREEN_H >> 1);
    g->ship_vx = 0;
    g->ship_vy = 0;
    g->ship_ex = 0;
    g->ship_ey = 0;
    g->turn_cd = 0;
    g->friction_cd = 0;
    g->thrusting = 0;
    g->ship_dir = 0;
}

static void spawn_asteroid(struct game_state *g, uint8_t size) {
    uint8_t i;
    uint16_t left_x = k_ast_left_x[size];
    uint16_t right_x = k_ast_right_x[size];
    uint16_t top_y = k_ast_top_y[size];
    uint16_t bottom_y = k_ast_bottom_y[size];
    for (i = 0; i < AST_MAX; ++i) {
        struct asteroid_rec *a = &g->asteroids[i];
        if (!a->active) {
            uint8_t edge  = (uint8_t)rand_mask(g, 3);
            uint8_t dir   = (uint8_t)rand_mask(g, DIR_MASK);
            uint8_t speed = (uint8_t)(AST_SPEED_MIN +
                                      rand_mask(g, (AST_SPEED_SPAN - 1)));
            if (edge == 0) {
                a->x = left_x;
                a->y = rand_inset_y(g, (uint16_t)top_y, (uint16_t)bottom_y);
            } else if (edge == 1) {
                a->x = right_x;
                a->y = rand_inset_y(g, (uint16_t)top_y, (uint16_t)bottom_y);
            } else if (edge == 2) {
                a->x = rand_inset_x(g, (uint16_t)left_x, (uint16_t)right_x);
                a->y = top_y;
            } else {
                a->x = rand_inset_x(g, (uint16_t)left_x, (uint16_t)right_x);
                a->y = bottom_y;
            }
            a->vx = k_dir_off_x[speed << 1][dir];
            a->vy = k_dir_off_y[speed << 1][dir];
            if (a->vx == 0 && a->vy == 0) a->vx = 3;
            a->ex = 0;
            a->ey = 0;
            a->shape = (uint8_t)rand_mask(g, (AST_TEMPLATE_COUNT - 1));
            a->angle = (uint8_t)rand_mask(g, DIR_MASK);
            a->size   = size;
            a->active = 1;
            if (g->ast_count < AST_MAX) g->ast_idx[g->ast_count++] = i;
            ++g->active_asteroids;
            return;
        }
    }
}

static struct asteroid_rec *find_free_asteroid(struct game_state *g) {
    uint8_t i;
    for (i = 0; i < AST_MAX; ++i) {
        if (!g->asteroids[i].active)
            return &g->asteroids[i];
    }
    return (struct asteroid_rec *)0;
}

static void spawn_wave(struct game_state *g) {
    uint8_t i;
    uint8_t count = (uint8_t)(AST_START + (g->wave >> 1));
    if (count > 14) count = 14;
    for (i = 0; i < count; ++i)
        spawn_asteroid(g, 3);
}

static void fire_bullet(struct game_state *g) {
    uint8_t i;
    uint8_t dir = g->ship_dir;
    uint8_t nose_y = DRAW_Y_RADIUS(SHIP_NOSE);
    uint8_t bullet_y = DRAW_Y_RADIUS(BULLET_SPEED);
    if (g->fire_cd > 0) return;
    for (i = 0; i < BULLET_MAX; ++i) {
        struct bullet_rec *b = &g->bullets[i];
        if (!b->active) {
            b->x    = wrap_x_step(g->ship_x, k_dir_off_x[SHIP_NOSE][dir]);
            b->y    = wrap_y_step(g->ship_y, k_dir_off_y[nose_y][dir]);
            b->dx   = k_dir_off_x[BULLET_SPEED][dir];
            b->dy   = k_dir_off_y[bullet_y][dir];
            b->life = BULLET_LIFE;
            b->active = 1;
            g->fire_cd = FIRE_COOLDOWN;
            return;
        }
    }
}

static void split_asteroid(struct game_state *g, struct asteroid_rec *a) {
    int16_t vx    = a->vx;
    int16_t vy    = a->vy;
    uint8_t nsize = (uint8_t)(a->size - 1);
    if (a->size <= 1) return;
    {
        uint8_t k;
        for (k = 0; k < 2; ++k) {
            struct asteroid_rec *n = find_free_asteroid(g);
            if (n) {
                int16_t tweak = (int16_t)(3 + rand_mod3(g));
                n->x = a->x;
                n->y = a->y;
                if (k == 0) {
                    n->vx = (int8_t)(vy + tweak);
                    n->vy = (int8_t)(-vx + 1);
                } else {
                    n->vx = (int8_t)(-vy - tweak);
                    n->vy = (int8_t)(vx - 1);
                }
                n->ex = 0;
                n->ey = 0;
                n->shape = (uint8_t)rand_mask(g, (AST_TEMPLATE_COUNT - 1));
                n->angle = (uint8_t)rand_mask(g, DIR_MASK);
                n->size   = nsize;
                n->active = 1;
                if (g->ast_count < AST_MAX)
                    g->ast_idx[g->ast_count++] = (uint8_t)(n - g->asteroids);
                ++g->active_asteroids;
            }
        }
    }
}

static void update_player(struct game_state *g, int key) {
    if (g->explode_timer > 0) return;
    g->thrusting = 0;
    if (g->turn_cd > 0) --g->turn_cd;
    if (key == KEY_LEFT) {
        if (g->turn_cd == 0) {
            g->ship_dir = (uint8_t)((g->ship_dir + DIR_MASK) & DIR_MASK);
            g->turn_cd  = SHIP_TURN_DELAY;
        }
    } else if (key == KEY_RIGHT) {
        if (g->turn_cd == 0) {
            g->ship_dir = (uint8_t)((g->ship_dir + 1) & DIR_MASK);
            g->turn_cd  = SHIP_TURN_DELAY;
        }
    } else if (key == KEY_UP || key == KEY_W) {
        uint8_t dir = g->ship_dir;
        g->thrusting = 1;
        g->ship_vx = (int8_t)(g->ship_vx + k_dir_off_x[8][dir]);
        g->ship_vy = (int8_t)(g->ship_vy + k_dir_off_y[8][dir]);
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
            if (g->ship_vx > 0)      g->ship_vx = (int8_t)(g->ship_vx - SHIP_FRICTION);
            else if (g->ship_vx < 0) g->ship_vx = (int8_t)(g->ship_vx + SHIP_FRICTION);
            if (g->ship_vy > 0)      g->ship_vy = (int8_t)(g->ship_vy - SHIP_FRICTION);
            else if (g->ship_vy < 0) g->ship_vy = (int8_t)(g->ship_vy + SHIP_FRICTION);
            g->friction_cd = SHIP_FRICTION_DELAY;
        }
    }
    g->ship_vx = clamp8(g->ship_vx, -SHIP_MAX_V, SHIP_MAX_V);
    g->ship_vy = clamp8(g->ship_vy, -SHIP_MAX_V, SHIP_MAX_V);
    g->ship_x  = move_bres_x(g->ship_x, g->ship_vx, &g->ship_ex);
    g->ship_y  = move_bres_y(g->ship_y, g->ship_vy, &g->ship_ey);
}

static void update_bullets(struct game_state *g) {
    uint8_t i;
    g->bullet_count = 0;
    for (i = 0; i < BULLET_MAX; ++i) {
        struct bullet_rec *b = &g->bullets[i];
        if (b->active) {
            b->x = wrap_x_step(b->x, b->dx);
            b->y = wrap_y_step(b->y, b->dy);
            if (--b->life == 0) b->active = 0;
            if (b->active)
                g->bullet_idx[g->bullet_count++] = i;
        }
    }
}

static void update_asteroids(struct game_state *g) {
    uint8_t i;
    g->ast_count = 0;
    for (i = 0; i < AST_MAX; ++i) {
        struct asteroid_rec *a = &g->asteroids[i];
        if (a->active) {
            a->x = move_bres_fast_x(a->x, a->vx, &a->ex);
            a->y = move_bres_fast_y(a->y, a->vy, &a->ey);
            a->x = wrap_inset_x(a->x, a->size);
            a->y = wrap_inset_y(a->y, a->size);
            a->angle = (uint8_t)((a->angle + 1) & DIR_MASK);
            g->ast_idx[g->ast_count++] = i;
        }
    }
}

static void check_bullet_hits(struct game_state *g) {
    uint8_t bi;
    for (bi = 0; bi < g->bullet_count; ++bi) {
        uint8_t bidx = g->bullet_idx[bi];
        struct bullet_rec *b = &g->bullets[bidx];
        if (!b->active) continue;
        {
            uint8_t ai;
            for (ai = 0; ai < g->ast_count; ++ai) {
                uint8_t aidx = g->ast_idx[ai];
                struct asteroid_rec *a = &g->asteroids[aidx];
                if (a->active) {
                    uint8_t r = k_ast_hit_radius[a->size];
                    uint16_t dx = wrap_abs_dx(b->x, a->x);
                    uint16_t dy = wrap_abs_dy(b->y, a->y);
                    if (dx < r && dy < r) {
                        b->active = 0;
                        a->active = 0;
                        --g->active_asteroids;
                        g->score = (uint16_t)(g->score + (uint16_t)((4 - a->size) << 3));
                        split_asteroid(g, a);
                        break;
                    }
                }
            }
        }
    }
}

static void check_ship_hits(struct game_state *g) {
    uint8_t ai;
    if (g->explode_timer > 0) return;
    for (ai = 0; ai < g->ast_count; ++ai) {
        uint8_t aidx = g->ast_idx[ai];
        struct asteroid_rec *a = &g->asteroids[aidx];
        if (a->active) {
            uint8_t r = k_ast_ship_hit_radius[a->size];
            uint16_t dx = wrap_abs_dx(g->ship_x, a->x);
            uint16_t dy = wrap_abs_dy(g->ship_y, a->y);
            if (dx < r && dy < r) {
                g->explode_x     = g->ship_x;
                g->explode_y     = g->ship_y;
                g->explode_timer = 12;
                if (g->lives > 0) --g->lives;
                if (g->lives == 0) g->over = 1;
                reset_ship(g);
                return;
            }
        }
    }
}

static void game_reset(struct game_state *g) {
    uint8_t i;
    g->started      = 1;
    g->score        = 0;
    g->wave         = 1;
    g->lives        = 3;
    g->fire_cd      = 0;
    g->turn_cd      = 0;
    g->friction_cd  = 0;
    g->over         = 0;
    g->explode_timer = 0;
    g->fire_queued  = 0;
    g->thrusting    = 0;
    g->bullet_count = 0;
    g->ast_count    = 0;
    g->active_asteroids = 0;
    for (i = 0; i < BULLET_MAX; ++i) g->bullets[i].active   = 0;
    for (i = 0; i < AST_MAX;    ++i) g->asteroids[i].active = 0;
    reset_ship(g);
    spawn_wave(g);
}

static void show_title_page(struct game_state *g) {
    uint8_t i;
    g->started       = 0;
    g->score         = 0;
    g->wave          = 1;
    g->lives         = 3;
    g->fire_cd       = 0;
    g->turn_cd       = 0;
    g->friction_cd   = 0;
    g->over          = 0;
    g->explode_timer = 0;
    g->fire_queued   = 0;
    g->thrusting     = 0;
    g->bullet_count  = 0;
    g->ast_count     = 0;
    g->active_asteroids = 0;
    for (i = 0; i < BULLET_MAX; ++i) g->bullets[i].active = 0;
    for (i = 0; i < AST_MAX; ++i) g->asteroids[i].active = 0;
    reset_ship(g);
}

/* ---------- HUD (always drawn with CO_FORE already set) ---------- */

static void draw_hud(struct game_state *g) {
    char buf[40];
    uint8_t pos, i;
    rect_t r;

    pos = 0;
    buf[pos++] = 'R'; buf[pos++] = 'E'; buf[pos++] = 'Z';
    buf[pos++] = 'U'; buf[pos++] = 'L'; buf[pos++] = 'T';
    buf[pos++] = 'A'; buf[pos++] = 'T'; buf[pos++] = ':'; buf[pos++] = ' ';
    pos = append_u16(buf, pos, g->score);
    buf[pos++] = ' ';
    buf[pos++] = ' ';
    buf[pos++] = 'V'; buf[pos++] = 'A'; buf[pos++] = 'L';
    buf[pos++] = ':'; buf[pos++] = ' ';
    pos = append_u16(buf, pos, g->wave);
    buf[pos++] = ' ';
    buf[pos++] = ' ';
    buf[pos++] = 'P'; buf[pos++] = 'O'; buf[pos++] = 'S';
    buf[pos++] = 'K'; buf[pos++] = 'U'; buf[pos++] = 'S';
    buf[pos++] = 'I'; buf[pos++] = ':'; buf[pos++] = ' ';
    for (i = 0; i < g->lives && i < 3; ++i) buf[pos++] = '*';
    while (pos < 36) buf[pos++] = ' ';
    buf[pos] = '\0';
    r.x0 = 0;
    r.y0 = 0;
    r.x1 = HUD_W;
    r.y1 = HUD_H;
    gsetcolor(CO_BACK);
    gfillrect(&r);
    gsetcolor(CO_FORE);
    gputtext(&idp8x16_font, buf, HUD_X, HUD_Y);
}

static void draw_centered(char *text, coord y) {
    dim_t  dim;
    coord  x;
    gmetext(&idp8x16_font, text, &dim);
    x = (SCREEN_W - dim.w) >> 1;
    if (x < 0) x = 0;
    gputtext(&idp8x16_font, text, x, y);
}

static void draw_game_over(struct game_state *g) {
    int16_t cy = (int16_t)(SCREEN_H >> 1);
    (void)g;
    draw_centered("KONEC IGRE",                  (coord)(cy - 28));
    draw_centered("PRITISNI ENTER ZA NOVO IGRO", (coord)(cy -  8));
    draw_centered("PRITISNI Q ZA IZHOD",         (coord)(cy + 12));
}

static void draw_kamenje_logo(coord x0, coord y0) {
    uint16_t row;
    for (row = 0; row < KAMENJE_LOGO_H; ++row) {
        uint16_t si;
        for (si = k_kamenje_logo_row_off[row];
             si < k_kamenje_logo_row_off[row + 1];
             ++si) {
            coord x1 = (coord)(x0 + k_kamenje_logo_spans[si].x0);
            coord x2 = (coord)(x0 + k_kamenje_logo_spans[si].x1);
            gxy(x1, (coord)(y0 + row));
            gdrawd((coord)(x2 - x1), 0);
        }
    }
}

static void draw_title_page(void) {
    coord x = (coord)((SCREEN_W - KAMENJE_LOGO_W) >> 1);
    coord y = 56;
    draw_kamenje_logo(x, y);
    draw_centered("PRITISNITE ENTER ZA START, Q ZA IZHOD.",
                  (coord)(y + KAMENJE_LOGO_H + 24));
}

/* ---------- frame ---------- */

static void frame(int key) {
    struct page_rec *pr = &pg_rec[write_pg];
    uint8_t i;

    if (key == KEY_QUIT) {
        g.running = 0;
        return;
    }

    /* ---- ERASE PHASE ---- */
    if (pr->valid) {
        rect_t r;

        gsetcolor(CO_BACK);

        /* erase game-over text area if it was shown */
        if (pr->game_over_shown) {
            r.x0 = GOVER_X0; r.y0 = GOVER_Y0;
            r.x1 = GOVER_X1; r.y1 = GOVER_Y1;
            gfillrect(&r);
        }
        if (pr->title_shown) {
            r.x0 = TITLE_X0; r.y0 = TITLE_Y0;
            r.x1 = TITLE_X1; r.y1 = TITLE_Y1;
            gfillrect(&r);
        }

        /* erase vector objects with CO_BACK lines */
        if (pr->ship_visible)
            (void)render_ship(pr->ship_sx, pr->ship_sy,
                              pr->ship_dir, pr->ship_thrusting);
        if (pr->exp_active)
            (void)render_explosion(pr->exp_cx, pr->exp_cy,
                                   pr->exp_inner, pr->exp_outer);
        for (i = 0; i < pr->bul_count; ++i)
            render_bullet(pr->bul[i].x, pr->bul[i].y);
        for (i = 0; i < pr->ast_count; ++i)
            render_asteroid(pr->ast[i].cx, pr->ast[i].cy,
                            pr->ast[i].size, pr->ast[i].shape,
                            pr->ast[i].angle);
    }

    /* ---- UPDATE PHASE ---- */
    if (!g.started) {
        if (key == KEY_ENTER)
            game_reset(&g);
    } else if (g.over) {
        if (key == KEY_ENTER)
            game_reset(&g);
    } else {
        if (g.fire_cd > 0)       --g.fire_cd;
        if (g.explode_timer > 0) --g.explode_timer;
        update_player(&g, key);
        update_bullets(&g);
        update_asteroids(&g);
        check_bullet_hits(&g);
        check_ship_hits(&g);
        if (g.active_asteroids == 0) {
            ++g.wave;
            spawn_wave(&g);
        }
    }

    /* ---- DRAW PHASE ---- */
    gsetcolor(CO_FORE);

    /* reset this page's record */
    pr->ship_visible   = 0;
    pr->exp_active     = 0;
    pr->game_over_shown = 0;
    pr->title_shown    = 0;
    pr->bul_count = 0;
    pr->ast_count = 0;

    if (g.started && (!pr->valid || pr->hud_score != g.score ||
        pr->hud_wave != g.wave || pr->hud_lives != g.lives)) {
        draw_hud(&g);
        pr->hud_score = g.score;
        pr->hud_wave = g.wave;
        pr->hud_lives = g.lives;
    }

    if (!g.started) {
        draw_title_page();
        pr->title_shown = 1;
    } else if (g.over) {
        draw_game_over(&g);
        pr->game_over_shown = 1;
    } else {
        /* ship or explosion */
        if (g.explode_timer == 0) {
            int16_t sx = (int16_t)g.ship_x;
            int16_t sy = (int16_t)g.ship_y;
            if (render_ship(sx, sy, g.ship_dir, g.thrusting)) {
                pr->ship_visible   = 1;
                pr->ship_sx        = sx;
                pr->ship_sy        = sy;
                pr->ship_dir       = g.ship_dir;
                pr->ship_thrusting = g.thrusting;
            }
        } else {
            int16_t cx    = (int16_t)g.explode_x;
            int16_t cy    = (int16_t)g.explode_y;
            uint8_t step  = (uint8_t)(12 - g.explode_timer);
            int16_t inner = (int16_t)(step << 1);
            int16_t outer;
            outer = (int16_t)(inner + 7);
            if (render_explosion(cx, cy, inner, outer)) {
                pr->exp_active = 1;
                pr->exp_cx     = cx;    pr->exp_cy    = cy;
                pr->exp_inner  = inner; pr->exp_outer = outer;
            }
        }

        /* bullets */
        for (i = 0; i < g.bullet_count; ++i) {
            uint8_t bi = g.bullet_idx[i];
            if (g.bullets[bi].active) {
                int16_t x = (int16_t)g.bullets[bi].x;
                int16_t y = (int16_t)g.bullets[bi].y;
                render_bullet(x, y);
                pr->bul[pr->bul_count].x = x;
                pr->bul[pr->bul_count].y = y;
                ++pr->bul_count;
            }
        }

        /* asteroids */
        for (i = 0; i < g.ast_count; ++i) {
            struct asteroid_rec *a = &g.asteroids[g.ast_idx[i]];
            if (a->active) {
                int16_t cx = (int16_t)a->x;
                int16_t cy = (int16_t)a->y;
                render_asteroid(cx, cy, a->size, a->shape, a->angle);
                pr->ast[pr->ast_count].cx    = cx;
                pr->ast[pr->ast_count].cy    = cy;
                pr->ast[pr->ast_count].angle = a->angle;
                pr->ast[pr->ast_count].size  = a->size;
                pr->ast[pr->ast_count].shape = a->shape;
                ++pr->ast_count;
            }
        }
    }

    pr->valid = 1;
}

/* ---------- entry point ---------- */

void game_run(void) {
    int key;

    clear_text_screen();
    hide_text_cursor();
    ginit(RES_1024x512);
    init_shape_cache();

    /* clear both pages */
    gsetpage(PG_DISPLAY | PG_WRITE, 0);
    gcls();
    gsetpage(PG_DISPLAY | PG_WRITE, 1);
    gcls();
    gsetpage(PG_DISPLAY, 0);
    display_pg = 0;
    write_pg   = 1;

    g.running   = 1;
    g.rng       = 0x5AA5u;
    show_title_page(&g);

    while (g.running) {
        key = read_key();

        write_pg = (uint8_t)(display_pg ^ 1);
        gsetpage(PG_WRITE, write_pg);

        frame(key);

        gsetpage(PG_DISPLAY, write_pg);
        display_pg = write_pg;
    }

    gsetpage(PG_DISPLAY | PG_WRITE, 0);
    gcls();
    gexit();
    show_text_cursor();
}
