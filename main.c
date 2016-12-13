#include "image.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char *readFileToString(const char *filename, size_t *length);
static void printUsage();
static void printHelp();

int main(int argc, char **argv){
    int ret = 0;

    //parse arguments
    if (argc == 2 && !strcmp(argv[1], "-h")){
        printHelp();
        return 0;
    } else if (argc == 1 || argc > 3){
        printUsage();
        return 1;
    }

    //read in from file
    size_t length;
    char *data = readFileToString(argv[1], &length);
    if (data == NULL){
        printf("Problem reading file\n");
        return 1;
    }
    Image *im = createImage(data, length);
    free(data);

    //do the processing on the images
    Image *left, *right;
    correctImage(im, &left, &right);

    //save the files out - allocate for directory, name, "-l" and "-r" suffix, delimiter, and null character
    size_t position;
    char *outputName;
    if (argc != 2){ //directory given
        position = strlen(argv[2]);
        outputName = calloc(strlen(argv[2]) + strlen(argv[1]) + 4, sizeof(char));
        strcpy(outputName, argv[2]);
        outputName[position] = '/';
        position++;
        strcpy(outputName + position, argv[1]);
        position += strlen(argv[1]) - 4;
    } else { //no directory
        outputName = calloc(strlen(argv[1]) + 3, sizeof(char));
        strcpy(outputName, argv[1]);
        position = strlen(argv[1]) - 4;
    }
    strcpy(outputName + position, "-l.pbm");
    if (!savePBM(left, outputName)){
        printf("Problem saving %s\n", outputName);
        ret = 1;
    }
    strcpy(outputName + position, "-r.pbm");
    if (!savePBM(right, outputName)){
        printf("Problem saving %s\n", outputName);
        ret = 1;
    }

    //free stuff
    free(left->data);
    free(right->data);
    free(im->data);
    free(left);
    free(right);
    free(im);
    free(outputName);

    return ret;
}

//read file into a string, the length of which is stored in length
char *readFileToString(const char *filename, size_t *length){
    FILE *fin;
    size_t len;
    char *data;

    //open file and compute length
    fin = fopen(filename, "rb");
    if (fin == NULL){
        return NULL;
    }
    fseek(fin, 0, SEEK_END);
    len = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    //allocate and read data
    data = malloc((len + 1) * sizeof(char));
    fread(data, 1, len, fin);
    data[len] = '\0';

    //close, set length, and return
    fclose(fin);
    *length = len;
    return data;
}

void printUsage(){
    printf("Usage:\n\tpbmcorrect <input file> [output directory]\n");
}

void printHelp(){
    printUsage();
    printf("Process the input PBM file into two PBM files in the output directory that are\nthe left and right page of the original file, rotated and centered.");
}
