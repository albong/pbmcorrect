#include "image.h"
#include <stdlib.h>
#include <stdio.h>

static char *readFileToString(const char *filename, size_t *length);

int main(int argc, char **argv){
    size_t length;
    char *data = readFileToString("upnt-003.pbm", &length);
    Image *im = createImage(data, length);
    free(data);
    Image *left, *right;
    correctImage(im, &left, &right);
    save(left, "lpage.pbm");
    save(right, "rpage.pbm");
    return 0;
}

char *readFileToString(const char *filename, size_t *length){
    FILE *fin;
    size_t len;
    char *data;

    fin = fopen(filename, "rb");
    fseek(fin, 0, SEEK_END);
    len = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    data = malloc((len + 1) * sizeof(char));

    fread(data, 1, len, fin);
    data[len] = '\0';
    fclose(fin);

    *length = len;
    return data;
}
