
# gj_image

## Supported file types
- PNG - Partial support
- BMP - Partial support

## Output format
- 8-bit per channel
- RGB|RGBA

## Build
```bash
make
```

## Example
```c
#include "gj_image/gj_image.h"

int main() {
    int width, height, channels;
    unsigned char *data = gj_image_load("image.bmp", &width, &height,
                                        &channels);

    if (!data) return 1;

    gj_image_free(data);

    return 0;
}
```
