#include "colorText.h"

#include <windows.h>
#include <stdlib.h>

// should color be used
static bool colorEnabled = true;

// can color be used
static bool colorFailed = false;

// standard output
static HANDLE stdOutHandle;

// default console settings from before running the program
static CONSOLE_SCREEN_BUFFER_INFO stdOutDefaultSettings;

// initialise global variables
void initialiseColor() {
    stdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if(!GetConsoleScreenBufferInfo(stdOutHandle, &stdOutDefaultSettings)) {
        colorFailed = true;
        colorEnabled = false;
    }

    // prevent the following programs from having different colored text
    atexit(resetColor);
}

// change color enabled state
void setColorEnabled(bool isEnabled) {
    colorEnabled = isEnabled & !colorFailed;
}

// set a color to print with
void setColor(TextColor color) {
    if(colorEnabled) {
        SetConsoleTextAttribute(stdOutHandle, color);
    }
}

// return to the default color
void resetColor() {
    if(colorEnabled) {
        SetConsoleTextAttribute(stdOutHandle, stdOutDefaultSettings.wAttributes);
    }
}