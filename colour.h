#ifndef _COLOUR_H
#define _COLOUR_H

typedef struct RGBAColour
{
    int r;
    int g;
    int b;
    int a;
} RGBAColour;

// MMA colour table 55
#define NCOLOURTABLES 100
#define NCOLOURS 50

RGBAColour colourFromTable(int tableId, int colourIndex);

RGBAColour colourFromString(char *colourString);

#endif // _COLOUR_H
