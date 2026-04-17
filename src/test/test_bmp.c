#include "formats/bmp.h"
#include "common/common.h"
#include "test/display/display.h"
#include "gj_image/gj_image.h"
#include <stdio.h>

int test_bmp(const char *filename) {
    int width, height, channels;
    struct image_file image_file = {
        .filename = filename,
        .width = &width,
        .height = &height,
        .channels = &channels
    };
    unsigned char *data = bmp_open(&image_file);
    if (!data) {
        printf("FAILED IAMGE LOAD\n");
        return -1;
    }
    display_image(data, *image_file.width, *image_file.height, *image_file.channels);
    gj_image_free(data);
    return 0;
}


int main() {
    test_bmp("assets/nar.bmp");
    return 0;
}
