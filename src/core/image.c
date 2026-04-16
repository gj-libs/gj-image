#include <stdio.h>
#include "gj_image/gj_image.h"

unsigned char *gj_image_load(const char *filename, int *width, int *height, int *channels) {
    printf("gl_image_load called\n");
    return NULL;
}

void gj_image_free(unsigned char *data) {
    printf("gl_image_free called\n");
}
