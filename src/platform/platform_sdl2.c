#include "platform.h"

#include <SDL2/SDL.h>

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

#define PARTNER_WIDTH 1024
#define PARTNER_HEIGHT 512
#define FONT_W 5
#define FONT_H 7
#define FONT_SPACING 1

struct Glyph {
    char c;
    uint8_t rows[FONT_H];
};

static const struct Glyph k_glyphs[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {':', {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00}},
    {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1E, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E}},
    {'6', {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x10, 0x13, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F}},
    {'J', {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}}
};

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static int g_draw_color = FORE;

static void set_sdl_color_from_platform(int color) {
    if (color == BACK) {
        (void) SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    } else {
        (void) SDL_SetRenderDrawColor(g_renderer, 255, 255, 255, 255);
    }
}

static const uint8_t *find_glyph(char c) {
    size_t i;
    char up = (char) toupper((unsigned char) c);

    for (i = 0; i < sizeof(k_glyphs) / sizeof(k_glyphs[0]); ++i) {
        if (k_glyphs[i].c == up) {
            return k_glyphs[i].rows;
        }
    }

    return k_glyphs[0].rows;
}

static int map_key(SDL_Keycode code) {
    if (code == SDLK_UP) {
        return KEY_UP;
    }
    if (code == SDLK_DOWN) {
        return KEY_DOWN;
    }
    if (code == SDLK_LEFT) {
        return KEY_LEFT;
    }
    if (code == SDLK_RIGHT) {
        return KEY_RIGHT;
    }
    if (code == SDLK_RETURN || code == SDLK_KP_ENTER) {
        return KEY_ENTER;
    }
    if (code == SDLK_SPACE) {
        return KEY_SPACE;
    }
    if (code == SDLK_w) {
        return KEY_W;
    }
    if (code == SDLK_s) {
        return KEY_S;
    }
    if (code == SDLK_q || code == SDLK_ESCAPE) {
        return KEY_QUIT;
    }

    return KEY_NONE;
}

bool platform_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        return false;
    }

    g_window = SDL_CreateWindow("Iskra Delta Asteroids",
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                PARTNER_WIDTH,
                                PARTNER_HEIGHT,
                                SDL_WINDOW_SHOWN);
    if (!g_window) {
        SDL_Quit();
        return false;
    }

    g_renderer = SDL_CreateRenderer(g_window,
                                    -1,
                                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!g_renderer) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
        SDL_Quit();
        return false;
    }

    (void) SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    (void) SDL_RenderSetLogicalSize(g_renderer, PARTNER_WIDTH, PARTNER_HEIGHT);

    g_draw_color = BACK;
    set_sdl_color_from_platform(g_draw_color);
    (void) SDL_RenderClear(g_renderer);
    SDL_RenderPresent(g_renderer);
    g_draw_color = FORE;
    set_sdl_color_from_platform(g_draw_color);

    return true;
}

void platform_shutdown(void) {
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
    SDL_Quit();
}

void platform_cls(void) {
    set_sdl_color_from_platform(g_draw_color);
    (void) SDL_RenderClear(g_renderer);
}

void platform_set_color(int color) {
    g_draw_color = (color == BACK) ? BACK : FORE;
    set_sdl_color_from_platform(g_draw_color);
}

void platform_draw_line(int x0, int y0, int x1, int y1) {
    (void) SDL_RenderDrawLine(g_renderer, x0, y0, x1, y1);
}

static void draw_text_scaled(int x, int y, const char *text, int scale) {
    int i;

    if (!text || scale < 1) {
        return;
    }

    for (i = 0; text[i] != '\0'; ++i) {
        int row;
        const uint8_t *glyph = find_glyph(text[i]);
        int base_x = x + (i * (FONT_W + FONT_SPACING) * scale);

        for (row = 0; row < FONT_H; ++row) {
            int col;
            uint8_t bits = glyph[row];
            for (col = 0; col < FONT_W; ++col) {
                int mask = 1 << (FONT_W - 1 - col);
                if ((bits & (uint8_t) mask) != 0) {
                    SDL_Rect px;
                    px.x = base_x + (col * scale);
                    px.y = y + (row * scale);
                    px.w = scale;
                    px.h = scale;
                    (void) SDL_RenderFillRect(g_renderer, &px);
                }
            }
        }
    }
}

void platform_draw_text(int x, int y, const char *text) {
    draw_text_scaled(x, y, text, 1);
}

void platform_draw_text2x(int x, int y, const char *text) {
    draw_text_scaled(x, y, text, 2);
}

int platform_display_width(void) {
    return PARTNER_WIDTH;
}

int platform_display_height(void) {
    return PARTNER_HEIGHT;
}

int platform_read_key(void) {
    SDL_Event ev;

    while (SDL_PollEvent(&ev) != 0) {
        if (ev.type == SDL_QUIT) {
            return KEY_QUIT;
        }
        if (ev.type == SDL_KEYDOWN && ev.key.repeat == 0) {
            return map_key(ev.key.keysym.sym);
        }
    }

    {
        const Uint8 *state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_W]) {
            return KEY_W;
        }
        if (state[SDL_SCANCODE_S]) {
            return KEY_S;
        }
        if (state[SDL_SCANCODE_LEFT]) {
            return KEY_LEFT;
        }
        if (state[SDL_SCANCODE_RIGHT]) {
            return KEY_RIGHT;
        }
        if (state[SDL_SCANCODE_UP]) {
            return KEY_UP;
        }
        if (state[SDL_SCANCODE_DOWN]) {
            return KEY_DOWN;
        }
    }

    return KEY_NONE;
}

void platform_present(void) {
    SDL_RenderPresent(g_renderer);
}

void platform_delay(uint ms) {
    SDL_Delay(ms);
}

void platform_loop(void (*frame_fn)(int key, void *ctx),
                   int (*running_fn)(void *ctx),
                   void *ctx,
                   uint frame_delay_ms) {
    if (!frame_fn || !running_fn) {
        return;
    }

    while (running_fn(ctx)) {
        int key = platform_read_key();
        frame_fn(key, ctx);
        platform_present();
        platform_delay(frame_delay_ms);
    }
}
