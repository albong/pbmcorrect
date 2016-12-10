#ifndef IMAGE_H
#define IMAGE_H

#include <stdlib.h>

typedef struct Image {
    unsigned int width;
    unsigned int height;
    unsigned int numBytesPerRow;
    char *data;
} Image;

Image *createImage(char *pbmContents, size_t len);
int correctImage(Image *self, Image **lResult, Image **rResult);
int savePBM(Image *self, const char *filename);

#endif
