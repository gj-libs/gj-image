#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <stdlib.h>
#include "display.h"

// channels = 3 (RGB) or 4 (RGBA)
void display_image(unsigned char *pixels, int width, int height, int channels) {
    int safe_height = (height == 0) ? 1 : abs(height);
    int scale = 1000 / safe_height;
    if (scale < 1) scale = 1;

    int abs_w = width;
    int abs_h = abs(height);
    int win_w = abs_w * scale;
    int win_h = abs_h * scale;

    if (win_w <= 0 || win_h <= 0) return;
    if (win_w > 8000) win_w = 8000;
    if (win_h > 8000) win_h = 8000;

    uint8_t *scaled_data =
        malloc(win_w * win_h * 4);

    for (int y = 0; y < abs_h; y++) {
        for (int x = 0; x < abs_w; x++) {
            uint8_t r = pixels[(y*width + x)*channels + 0];
            uint8_t g = pixels[(y*width + x)*channels + 1];
            uint8_t b = pixels[(y*width + x)*channels + 2];
            uint8_t a = (channels == 4) ? pixels[(y*width + x)*channels + 3] : 255;

            // simple alpha over black
            r = (r * a) / 255;
            g = (g * a) / 255;
            b = (b * a) / 255;

            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    int sx = x*scale + dx;
                    int sy = y*scale + dy;
                    int off = (sy*(width*scale) + sx) * 4;

                    scaled_data[off + 0] = b;
                    scaled_data[off + 1] = g;
                    scaled_data[off + 2] = r;
                    scaled_data[off + 3] = 0;
                }
            }
        }
    }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        return;
    }

    int screen = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(
        dpy, RootWindow(dpy, screen),
        0, 0, win_w, win_h, 1,
        BlackPixel(dpy, screen),
        WhitePixel(dpy, screen)
    );

    XStoreName(dpy, win, "PNG Display");
    XSelectInput(dpy, win, ExposureMask | KeyPressMask);
    XMapWindow(dpy, win);

    GC gc = DefaultGC(dpy, screen);

    XEvent e;
    do { XNextEvent(dpy, &e); } while (e.type != Expose);

    int depth = DefaultDepth(dpy, screen);

    XImage *img = XCreateImage(
        dpy,
        DefaultVisual(dpy, screen),
        depth,
        ZPixmap,
        0,
        (char *)scaled_data,
        win_w,
        win_h,
        32,
        0
    );

    XPutImage(dpy, win, gc, img, 0, 0, 0, 0, win_w, win_h);
    XFlush(dpy);

    while (1) {
        XNextEvent(dpy, &e);
        if (e.type == KeyPress) break;
    }

    XDestroyImage(img);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
}
