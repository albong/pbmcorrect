#include "image.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <math.h>

//structs
typedef struct Pair {
    unsigned int x;
    unsigned int y;
} Pair;

//constants
static unsigned int MARGIN_SIZE = 10;
static int NUM_DILATIONS = 8;

//image processing methods
static void findSeamRange(Image *self, unsigned int rowNum, unsigned int *seamStart, unsigned int *seamEnd);
static void clearMargins(Image *self, unsigned int width);
static void dilate(Image *self);
static double findRotationAngle(Image *self);
static void rotate(Image *self, double theta);

//image access methods
static int get(Image *self, unsigned int x, unsigned int y);
static int set(Image *self, unsigned int x, unsigned int y, int val);
static void printBox(Image *self, unsigned int x, unsigned int y, unsigned int width, unsigned int height);
static Image *copyBox(Image *self, unsigned int x, unsigned int y, unsigned int width, unsigned int height);
static Image *copy(Image *self);
static int getSample(Image *self, double x, double y);

//utility methods
static int skipWhitespace(char *str, size_t start);
static int fitLineToPoints(Pair *points, size_t numPoints, double *mInvResult, double *bResult);
static size_t removeXOutliers(Pair *points, size_t len);
static void leftShift(char *data, unsigned int height, unsigned int numBytesPerRow);


Image *createImage(char *pbmContents, size_t length){
    size_t c, start;
    unsigned int width, height;
    char buffer[80];

    //parse the header
    //first, read the magic characters - P4
    c = skipWhitespace(pbmContents, 0);
    start = c;
    while (!isspace(pbmContents[c])){
        if ((c - start) == 79){
            printf("Bad header\n");
            return NULL;
        }
        buffer[c - start] = pbmContents[c];
        c++;
    }
    buffer[c - start] = '\0';
    if (strcmp(buffer, "P4")){
        printf("Wrong magic\n");
        return NULL;
    }

    //read the width
    c = skipWhitespace(pbmContents, c);
    start = c;
    while (!isspace(pbmContents[c])){
        if ((c - start) == 79){
            printf("Bad header\n");
            return NULL;
        }
        buffer[c - start] = pbmContents[c];
        c++;
    }
    buffer[c - start] = '\0';
    width = strtol(buffer, NULL, 10);
    if (errno == ERANGE){
        printf("Unable to parse width\n");
        errno = 0;
        return NULL;
    }

    //read the height
    c = skipWhitespace(pbmContents, c);
    start = c;
    while (!isspace(pbmContents[c])){
        if ((c - start) == 79){
            printf("Bad header\n");
            return NULL;
        }
        buffer[c - start] = pbmContents[c];
        c++;
    }
    buffer[c - start] = '\0';
    height = strtol(buffer, NULL, 10);
    if (errno == ERANGE){
        printf("Unable to parse height\n");
        errno = 0;
        return NULL;
    }

    //the next character is whitespace, and then we read the whole file to the end
    c++;
    start = c;
    unsigned int numBytesPerRow = (width / 8) + ((width % 8) != 0);
    char *contents = malloc(sizeof(char) * height * numBytesPerRow);
//    for (c; c < length; c++){
//        contents[c - start] = pbmContents[c];
//    }
    memcpy(contents, pbmContents+c, numBytesPerRow * height);

    //finally, allocate and fill the struct
    Image *result = malloc(sizeof(Image));
    result->width = width;
    result->height = height;
    result->numBytesPerRow = numBytesPerRow;
    result->data = contents;

    return result;
}


/////////////////////////////////////////////////
// Image processing methods
/////////////////////////////////////////////////

//takes an image, splits it, and then rotates each split page, storing the results in the given pointers
//return value is meaningless
int correctImage(Image *self, Image **lResult, Image **rResult){
    //find seam and split
    //clear margins
    //dilate
    //find margin from left
    //determine rotation angle
    //rotate

    Image *left, *right, *leftBloat, *rightBloat;
    unsigned int leftCrop, rightCrop;
    unsigned int seamStart, seamEnd;
    size_t i,j;

    //go row by row and dumbly choose where we think the seam starts/stops
    //so that we know hwere to crop
    leftCrop = -1;
    rightCrop = 0;
    for (j = 0; j < self->height; j++){
        findSeamRange(self, j, &seamStart, &seamEnd);
        if (seamStart != -1 && seamEnd != -1){
            if (seamStart < leftCrop){
                leftCrop = seamStart;
            }
            if (seamEnd > rightCrop){
                rightCrop = seamEnd;
            }
        }
    }
    left = copyBox(self, 0, 0, leftCrop + 1, self->height);
    right = copyBox(self, rightCrop, 0, self->width - rightCrop - 1, self->height);

    //clear margins - 10 pixels on each side - pizza hardcoded
    clearMargins(left, MARGIN_SIZE);
    clearMargins(right, MARGIN_SIZE);

    //rotate the split images
    double rotationAngle;
    rotationAngle = findRotationAngle(left);
    rotate(left, rotationAngle);
    rotationAngle = findRotationAngle(right);
    rotate(right, rotationAngle);

    //store results
    *lResult = left;
    *rResult = right;
    return 0;
}

//find where the middle seam starts and ends, store results in seamStart and seamEnd
void findSeamRange(Image *self, unsigned int rowNum, unsigned int *seamStart, unsigned int *seamEnd){
    unsigned int resultStart, resultEnd;
    unsigned int startX = self->width / 2;
    unsigned int leftShift = 0;
    unsigned int rightShift = 0;
    unsigned int moreShift = 0;

    //the three cases: 1) seam is in middle, 2) seam is to the left of middle, 3) seam is to the right of middle

    //case 1
    if (get(self, startX, rowNum)){
        
        //look left and right till find white pixel
        while(get(self, startX - leftShift, rowNum) || get(self, startX + rightShift, rowNum)){
            if (get(self, startX - leftShift, rowNum)){
                leftShift++;
            }
            if (get(self, startX + rightShift, rowNum)){
                rightShift++;
            }
        }

        //if overrun, just skip
        if (leftShift >= self->width || rightShift >= self->width){
            resultStart = -1;
            resultEnd = -1;
        } else {
            resultStart = startX - leftShift;
            resultEnd = startX + rightShift;
        }

    //cases 2 and 3
    } else {
        //look left and right till find black pixel
        while(!get(self, startX - leftShift, rowNum) && !get(self, startX + rightShift, rowNum) && leftShift <= self->width / 2){
            leftShift++;
            rightShift++;
        }

        // all white line
        if (leftShift > self->width / 2){
            resultStart = -1;
            resultEnd = -1; 
        } 

        //case 2
        else if (get(self, startX - leftShift, rowNum)){
            while (get(self, startX - leftShift - moreShift, rowNum)){
                moreShift++;
            }
            if (leftShift + moreShift > self->width / 2){
                resultStart = -1;
                resultEnd = -1; 
            } else {
                resultStart = startX - leftShift - moreShift;
                resultEnd = startX - leftShift;
            }
        }
 
        //case 3
        else {
            while (get(self, startX + rightShift + moreShift, rowNum)){
                moreShift++;
            }
            if (rightShift + moreShift > self->width / 2){
                resultStart = -1;
                resultEnd = -1; 
            } else {
                resultStart = startX + rightShift + moreShift;
                resultEnd = startX + rightShift;
            }
        }
    }

    *seamStart = resultStart;
    *seamEnd = resultEnd;
}

//set all pixels within width of the edge to white
void clearMargins(Image *self, unsigned int width){
    size_t i, j;
    for (i = 0; i < self->width; i++){
        for (j = 0; j < width; j++){
            set(self, i, j, 0);
            set(self, i, self->height - j - 1, 0);
        }
    }
    for (j = 0; j < self->height; j++){
        for (i = 0; i < width; i++){
            set(self, i, j, 0);
            set(self, self->width - i - 1, j, 0);
        }
    }
}

//dilate and image NUM_DILATION times - union of original, left shift, and up and down left diagonal shifts
void dilate(Image *self){
    char *centerPixels = malloc(sizeof(char) * self->numBytesPerRow * self->height);
    char *upPixels = malloc(sizeof(char) * self->numBytesPerRow * self->height);
    char *downPixels = malloc(sizeof(char) * self->numBytesPerRow * self->height);
    size_t count, i;

    for (count = 0; count < NUM_DILATIONS; count++){
        //create copy of image, copy shifted up, and copy shifted down - probably could streamline this
        memcpy(centerPixels, self->data, self->numBytesPerRow * self->height);
        memcpy(upPixels, self->data + self->numBytesPerRow, (self->numBytesPerRow - 1) * self->height);
        memcpy(downPixels + self->numBytesPerRow, self->data, (self->numBytesPerRow - 1) * self->height);

        //shift the copies left
        leftShift(centerPixels, self->height, self->numBytesPerRow);
        leftShift(upPixels, self->height, self->numBytesPerRow);
        leftShift(downPixels, self->height, self->numBytesPerRow);

        //propagate the changes to the original
        for (i = 0; i < self->height * self->numBytesPerRow; i++){
            self->data[i] |= centerPixels[i];
            self->data[i] |= upPixels[i];
            self->data[i] |= downPixels[i];
        }
    }

    free(centerPixels);
    free(upPixels);
    free(downPixels);
}

//determine the angle to rotate the image so that the margin is straight
double findRotationAngle(Image *self){
    size_t i, j;
    Image *dilatedSelf = copy(self);
    dilate(dilatedSelf);

    //find the equation for a line that matches up to the margin
    Pair *marginPoints = malloc(sizeof(Pair) * self->height);
    size_t numMarginPoints = 0;
    double mInv, b;
    for (j = 0; j < self->height; j++){
        for (i = 0; i < self->width / 4; i++){
            if (get(dilatedSelf, i, j)){
                marginPoints[numMarginPoints].x = i;
                marginPoints[numMarginPoints].y = j;
                numMarginPoints++;
                break;
            }
        }
    }
    
    //remove outliers and fit a line in terms of y to the margin
    numMarginPoints = removeXOutliers(marginPoints, numMarginPoints);
    fitLineToPoints(marginPoints, numMarginPoints, &mInv, &b);

    //no need for these anymore
    free(marginPoints);
    free(dilatedSelf->data);
    free(dilatedSelf);

    //if you were reasonably confident about how bad rotation could be you could just assign these without searching
    //doubles because Pair uses unsigned ints
    double x1, x2, y1, y2;
    for (j = 0; j < self->height; j++){
        x1 = (mInv * j) + b;
        if (0 <= x1 && x1 < self->width){
            y1 = j;
            break;
        }
    }
    for (j = self->height - 1; j >= 0; j--){
        x2 = (mInv * j) + b;
        if (0 <= x2 && x2 < self->width){
            y2 = j;
            break;
        }
    }
    double angle = atan(fabs(x1 - x2) / fabs(y1 - y2));
    if (mInv < 0){
        angle = -1 * angle;
    }
    return angle;
}

//rotate an image theta radians about the center of the image
static void rotate(Image *self, double theta){
    size_t i, j;
    double centerX = ((double)self->width) / 2;
    double centerY = ((double)self->height) / 2;
    double srcX, srcY, x, y;
    Image temp;

    //create a temp Image to store our new pixel data in
    temp.width = self->width;
    temp.height = self->height;
    temp.numBytesPerRow = self->numBytesPerRow;
    temp.data = malloc(sizeof(char) * temp.numBytesPerRow * temp.height);
    
    //set new pixels in temp
    for (j = 0; j < self->height; j++){
        for (i = 0; i < self->width; i++){
            x = (double)i;
            y = (double)j;
            srcX = (x-centerX)*cos(-1*theta) - (y-centerY)*sin(-1*theta) + centerX;
            srcY = (x-centerX)*sin(-1*theta) + (y-centerY)*cos(-1*theta) + centerY;
            
            if (0 <= srcX && srcX < self->width && 0 <= srcY && srcY < self->height){
                //set(&temp, i, j, get(self, (unsigned int)srcX, (unsigned int)srcY));
                set(&temp, i, j, getSample(self, srcX, srcY));
            } else {
                set(&temp, i, j, 0);
            }
        }
    }

    //copy and free the changes
    memcpy(self->data, temp.data, self->numBytesPerRow * self->height);
    free(temp.data);
}


/////////////////////////////////////////////////
// Image access methods
/////////////////////////////////////////////////

int get(Image *self, unsigned int x, unsigned int y){
    if (x >= self->width || y >= self->height){
        return 0;
    }
    size_t index = (self->numBytesPerRow * y) + (x / 8);
    return (self->data[index] >> (7 - (x % 8))) & 1;
}

//set the specified pixel to the specified value
int set(Image *self, unsigned int x, unsigned int y, int val){
    if (x >= self->width || y >= self->height){
        return -1;
    }

    //make sure val is 0 or 1
    val = !!val;
    size_t index = (self->numBytesPerRow * y) + (x / 8);
    //clear and then set the bit
    self->data[index] &= ~(1 << (7 - (x % 8)));
    self->data[index] |= (val << (7 - (x % 8)));
    return 0;
}

//returns a weighted average of the values for the 4 pixels about (x,y), with some basic rounding assumptions
//thanks to: http://www.leptonica.com/rotation.html
int getSample(Image *self, double x, double y){
    double xDec, yDec, xInt, yInt;

    //extract fractional and integer parts
    xDec = modf(x, &xInt);
    yDec = modf(y, &yInt);

    //compute the contribution of each neighbor pixel, and compute a decimal color for the new pixel
    double weightUL, weightUR, weightLL, weightLR, result;
    weightUL = get(self, xInt, yInt) * (1 - xDec) * (1 - yDec);
    weightUR = get(self, xInt + 1, yInt) * xDec * (1 - yDec);
    weightLL = get(self, xInt, yInt + 1) * (1 - xDec) * yDec;
    weightLR = get(self, xInt + 1, yInt + 1) * xDec * yDec;
    result = weightUL + weightUR + weightLL + weightLR;

    //round and return
    if (result < 0.5){
        return 0;
    } else {
        return 1;
    }
}


//print a subset of the image to the console
void printBox(Image *self, unsigned int x, unsigned int y, unsigned int width, unsigned int height){
    if (x + width > self->width || y + height > self->height){
        return;
    }
    unsigned int i, j;
    for (j = y; j < y + height; j++){
        for (i = x; i < x + width; i++){
            if (get(self, i, j)){
                printf(".");
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }
}

//return a new image consisting of the given rectangular subset of the image
Image *copyBox(Image *self, unsigned int x, unsigned int y, unsigned int width, unsigned int height){
    if (x + width > self->width || y + height > self->height){
        return NULL;
    }

    Image *result = malloc(sizeof(Image));
    result->width = width;
    result->height = height;
    result->numBytesPerRow = (width / 8) + (width % 8 != 0);
    result->data = malloc(result->numBytesPerRow * height * sizeof(char));
   
    size_t i, j;
    for (j = y; j < y + height; j++){
        for (i = x; i < x + width; i++){
            set(result, i - x, j - y, get(self, i, j));
        }
    }

    return result;
}

//return a copy of the image
Image *copy(Image *self){
    return copyBox(self, 0, 0, self->width, self->height);
}

//return 1 for succses, 0 for failure
int savePBM(Image *self, const char *filename){
    int ret;
    FILE *fout;
   
    fout = fopen(filename, "wb");
    if (fout == NULL){
        return 0;
    }
     
    //write the header
    ret = fprintf(fout, "P4\n%d %d\n", self->width, self->height);
    if (ret < 0){
        fclose(fout);
        return 0;
    }

    //write the data
    ret = fwrite(self->data, sizeof(char), self->numBytesPerRow * self->height, fout);
    if (ret != self->numBytesPerRow * self->height){
        fclose(fout);
        return 0;
    }

    fclose(fout);
    return 1;
}

/////////////////////////////////////////////////
// Utility methods
/////////////////////////////////////////////////

//return the next index in the string str that isn't whitespace, starting start
int skipWhitespace(char *str, size_t start){
    size_t curr = start;
    while (isspace(str[curr])){
        curr++;
    }
    return curr;
}

//go through the list of Pairs, compute x in terms of y (since for very straight margins, y in terms of x will have large slope)
//store resulting slope and x-intercept in given doubles
//thanks to: http://www.varsitytutors.com/hotmath/hotmath_help/topics/line-of-best-fit
static int fitLineToPoints(Pair *points, size_t numPoints, double *mInvResult, double *bResult){
    size_t i;
    double xAvg, yAvg;
    double sumNumerator, sumDenominator;
    
    //compute averages
    xAvg = 0.0;
    yAvg = 0.0;
    for (i = 0; i < numPoints; i++){
        xAvg = (xAvg * (((float)i) / (i + 1))) + (((double) points[i].x) / (i + 1));
        yAvg = (yAvg * (((float)i) / (i + 1))) + (((double) points[i].y) / (i + 1));
    }

    //find slope and x intercept (remember, x in terms of y)
    sumNumerator = 0.0;
    sumDenominator = 0.0;
    for (i = 0; i < numPoints; i++){
        sumNumerator += (points[i].y - yAvg) * (points[i].x - xAvg);
        sumDenominator += (points[i].y - yAvg) * (points[i].y - yAvg);
    }

    //store and return
    *mInvResult = sumNumerator / sumDenominator;
    *bResult = xAvg - ((*mInvResult) * yAvg);
    return 0;
}

//computes the average over the x-values and removes in place anything beyond 1/4th a std dev; returns new length
//thanks to: http://codeselfstudy.com/blogs/how-to-calculate-standard-deviation-in-python
size_t removeXOutliers(Pair *points, size_t len){
    double mean, sumSquareDifferences, stdDev;
    size_t i;

    //compute the average
    for (i = 0; i < len; i++){
        mean = (mean * (((float)i) / (i + 1))) + (((double) points[i].x) / (i + 1));
    }

    //compute the sum of the squares of the differences, then the std dev.
    sumSquareDifferences = 0.0;
    for (i = 0; i < len; i++){
        sumSquareDifferences += (((double)points[i].x) - mean) * (((double)points[i].x) - mean);
    }
    stdDev = sqrt(sumSquareDifferences / len);

    //copy over points within 1/4th a std dev
    double lowerBound = mean - (stdDev / 4);
    double upperBound = mean + (stdDev / 4);
    size_t newLength = 0;
    Pair *newPoints = malloc(sizeof(Pair) * len);
    for (i = 0; i < len; i++){
        if (lowerBound <= points[i].x && points[i].x <= upperBound){
            newPoints[newLength] = points[i];
            newLength++;
        }
    }

    //copy, free, and return
    memcpy(points, newPoints, sizeof(Pair) * newLength);
    free(newPoints);
    return newLength;
}

//shift the given array of image data to the left one bit
void leftShift(char *data, unsigned int height, unsigned int numBytesPerRow){
    size_t i;
    size_t length = height * numBytesPerRow;
    unsigned char bit1 = 0;
    unsigned char bit2;
    for (i = 0; i < length; i++){
        bit2 = ((data[length - i - 1] & (1 << 7))) >> 7;
        data[length - i - 1] <<= 1;
        data[length - i - 1] |= bit1;
        bit1 = bit2;
    }
}
