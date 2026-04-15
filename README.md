
# gj_image

## Supported file types
- PNG - Partial support
- BMP - Partial support

## Output format
- 8-bit per channel
- RGBA

## Build
```bash
make
```

## Example
```c
#include <gj_image.h>

int main() {
    Image* img = img_load("image.png");

    if (!img) return 1;

    img_save_png(img, "out.png");

    img_free(img);

    return 0;
}
```
