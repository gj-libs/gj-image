#include <stdlib.h>
#include <string.h>
#include "gj_image/gj_image.h"
#include "error/error.h"
#include "common/common.h"
#include "formats/bmp.h"
#include "formats/png.h"

unsigned char *gj_image_load(const char *filename, int *width, int *height, int *channels) {
    char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        gj_set_error("No file extension found in \"%s\"\n", filename);
        return NULL;
    }

    struct image_file image_file = {
        .filename = filename,
        .width = width,
        .height = height,
        .channels = channels
    };

    if (strcasecmp(ext, ".bmp") == 0) {
        return bmp_open(&image_file);
    } else if (strcasecmp(ext, ".png") == 0) {
        return png_open(&image_file);
    } else {
        gj_set_error("Unsupported file format \"%s\"\n", ext);
        return NULL;
    }
    return NULL;
}

void gj_image_free(unsigned char *data) {
    free(data);
}
