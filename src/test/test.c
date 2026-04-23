#include "test/display/display.h"
#include "gj_image/gj_image.h"
#include <stdio.h>

int test_image(const char *filename) {
    int width, height, channels;
    unsigned char *data = gj_image_load(filename, &width, &height, &channels);
    if (!data) {
        printf("Err: %s", gj_get_last_error());
        return -1; }
    // display_image(data, width, height, channels);
    gj_image_free(data);
    return 0;
}

int main() {
    test_image("assets/cv.bmp");
    // test_image("assets/anaconda.png");
    return 0;
}
