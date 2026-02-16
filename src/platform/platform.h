#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

typedef unsigned int uint;

enum PlatformColor {
    BACK = 0,
    FORE = 1
};

enum PlatformKey {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ENTER,
    KEY_SPACE,
    KEY_W,
    KEY_S,
    KEY_QUIT
};

bool platform_init(void);
void platform_shutdown(void);

void platform_cls(void);
void platform_set_color(int color);
void platform_draw_line(int x0, int y0, int x1, int y1);
void platform_draw_text(int x, int y, const char *text);
void platform_draw_text2x(int x, int y, const char *text);

int platform_display_width(void);
int platform_display_height(void);
int platform_read_key(void);

void platform_present(void);
void platform_delay(uint ms);
void platform_loop(void (*frame_fn)(int key, void *ctx),
                   int (*running_fn)(void *ctx),
                   void *ctx,
                   uint frame_delay_ms);

#endif
