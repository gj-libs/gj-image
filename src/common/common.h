#ifndef COMMON_H
#define COMMON_H

struct image_file {
    const char *filename;
    int *width;
    int *height;
    int *channels;
};

#endif
