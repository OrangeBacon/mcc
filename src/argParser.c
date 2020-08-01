#include "argParser.h"

#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>

typedef struct argParser argParser;
typedef struct argArgument argArgument;

// is a character representing a setting in the setting name
static bool isSigil(char c) {
    return c == '-' || c == '!';
}

// process and remove all sigils from all setting names
static void parseArgumentSigils(argArgument* arg) {
    const char* start = arg->name;
    while(isSigil(*start)) {
        switch(*start) {
            case '-': arg->isOption = true; break;
            case '!': arg->isRequired = true; break;
        }
        start++;
    }
    arg->name = start;
}

// emit an error
void argError(argParser* parser, const char* message, ...) {
    va_list args;
    va_start(args, message);

    parser->hasError = true;
    if(parser->argc < 0) {
        fprintf(stderr, "Error at end of parameters: ");
    } else {
        fprintf(stderr, "Error at parameter %d: ", parser->initialArgc - parser->argc);
    }
    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");

    va_end(args);
}

// run the callbacks relating to a particular option
static void invokeOption(argParser* parser, argArgument* arg, const char* name, int len) {
    if(arg == NULL) {
        argError(parser, "%.*s is not a valid option", len, name);
        return;
    }
    if(arg->isDone) {
        argError(parser, "Option %s has already been provided", len, name);
        return;
    }

    parser->currentArgument = arg;
    arg->callback(parser, arg->callbackCtx);
    arg->isRequired = false;
}

// parse something like -a or --long-arg
static void parseOption(argParser* parser, const char* current) {

    parser->argc--;
    parser->argv++;

    // always >= 2
    size_t len = strlen(current);

    if(current[1] == '-') {
        // parse --arg, atleast one leter avaliable, as this is not '--' arg
        argArgument* arg = tableGet(&parser->argumentTable, &current[2], len - 2);
        invokeOption(parser, arg, &current[2], len-2);

    } else {
        // parse -abc
        for(unsigned int i = 1; i < len; i++) {
            char name = current[i];

            parser->canGetArg = i == 1;
            parser->canGetInternalArg = len != 2;
            parser->hasGotArg = false;

            argArgument* arg = tableGet(&parser->shortArgTable, &name, 1);

            invokeOption(parser, arg, &name, 1);
            if(parser->hasError || parser->hasGotArg) break;
        }
    }
}

// parse positional argument
static void parsePosition(argParser* parser) {
    for(unsigned int i = 0; i < parser->settingCount; i++) {
        argArgument* arg = &parser->settings[i];
        if(!arg->isOption && !arg->isDone) {

            parser->canGetArg = true;
            parser->canGetInternalArg = false;
            parser->hasGotArg = false;

            invokeOption(parser, arg, arg->name, strlen(arg->name));
            return;
        }
    }

    argError(parser, "Could not find use for positional argument");
}

// main parser
bool parseArgs(argParser* parser) {
    unsigned int settingCount = 0;
    parser->initialArgc = parser->argc;

    TABLE_INIT(parser->argumentTable, argArgument*);
    TABLE_INIT(parser->shortArgTable, argArgument*);

    argArgument* currentArg = parser->settings;
    while(currentArg->name != NULL) {
        parseArgumentSigils(currentArg);
        if(currentArg->isOption) {
            TABLE_SET(parser->argumentTable, currentArg->name,
                strlen(currentArg->name), currentArg);
            TABLE_SET(parser->shortArgTable, &currentArg->shortName, 1, currentArg);
        }
        currentArg++;
        settingCount++;
    }
    parser->settingCount = settingCount;

    bool isParsingOptions = true;

    while(parser->argc > 0) {
        const char* current = parser->argv[0];

        if(isParsingOptions && strcmp(current, "--") == 0) {
            isParsingOptions = false;
            parser->argc--;
            parser->argv++;
            continue;
        }

        if(isParsingOptions && current[0] == '-' && current[1] != '\0') {
            // parse option
            parseOption(parser, current);
            if(parser->hasError) return true;
        } else {
            // parse positional
            parsePosition(parser);
            if(parser->hasError) return true;
        }
    }

    parser->argc = -1;
    for(unsigned int i = 0; i < settingCount; i++) {
        argArgument* arg = &parser->settings[i];
        if(arg->isRequired) {
            argError(parser, "missing required argument %s", arg->name);
        }
    }

    return parser->hasError;
}

// get next string value from the parser
const char* argNextString(struct argParser* parser, bool shouldEmitError) {
    if(!parser->canGetArg) {
        if(shouldEmitError) {
            argError(parser, "Cannot read string argument from compressed flags");
        }
        return NULL;
    }

    if(parser->canGetInternalArg) {
        parser->hasGotArg = true;
        return &parser->argv[-1][2];
    }

    if(parser->argc == 0) {
        if(shouldEmitError) {
            argError(parser, "Missing string argument for %s", parser->currentArgument->name);
        }
        return NULL;
    }

    parser->hasGotArg = true;

    parser->argc--;
    const char* ret = parser->argv[0];
    parser->argv++;
    return ret;
}

int argNextInt(struct argParser* parser, bool shouldEmitError, bool* didError) {
    if(!parser->canGetArg) {
        if(shouldEmitError) {
            argError(parser, "Cannot read numeric argument from compressed flags");
        }
        *didError = true;
        return 0;
    }

    if(parser->argc == 0 && !parser->canGetInternalArg) {
        if(shouldEmitError) {
            argError(parser, "Missing numeric argument for %s", parser->currentArgument->name);
        }
        *didError = true;
        return 0;
    }

    parser->hasGotArg = true;

    const char* arg = argNextString(parser, false);
    if(arg == NULL) {
        argError(parser, "Cannot parse NULL as integer");
        *didError = true;
        return 0;
    }

    char* end;
    errno = 0;
    intmax_t num = strtoimax(arg, &end, 0);

    if(num > INT_MAX || (errno == ERANGE && num == INTMAX_MAX)) {
        argError(parser, "Integer value too large");
        *didError = true;
        return 0;
    }
    if(num < INT_MIN || (errno == ERANGE && num == INTMAX_MIN)) {
        argError(parser, "Integer value too small");
        *didError = true;
        return 0;
    }

    if(*end != '\0') {
        if(shouldEmitError) {
            argError(parser, "Unable to parse value as integer");
        }
        *didError = true;
        return 0;
    }

    return num;
}

// callback to set boolean switch to true
void argSet(argParser* parser, void* ctx) {
    *(bool*)ctx = true;
    parser->currentArgument->isDone = true;
}

// callback to push value
void argPush(struct argParser* parser, void* ctx) {
    struct stringList* list = ctx;
    if(list->datas == NULL) {
        ARRAY_ALLOC(const char*, *list, data);
    }

    const char* str = argNextString(parser, true);
    if(str == NULL) return;

    ARRAY_PUSH(*list, data, str);
}