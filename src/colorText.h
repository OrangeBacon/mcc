#ifndef COLOR_TEXT_H
#define COLOR_TEXT_H

#include <stdbool.h>

typedef enum TextColor {
#ifdef _WIN32
    TextBlack = 0,
    TextBlue,
    TextGreen,
    TextCyan,
    TextRed,
    TextMagenta,
    TextYellow,
    TextWhite
#else
    TextBlack = 30,
    TextRed,
    TextGreen,
    TextYellow,
    TextBlue,
    TextMagenta,
    TextCyan,
    TextWhite
#endif
} TextColor;

void initialiseColor();
void setColorEnabled(bool isEnabled);

void setColor(TextColor color);
void resetColor();

#endif