#ifndef ARG_PARSER_H
#define ARG_PARSER_H

#include <stdbool.h>
#include "memory.h"
#include "symbolTable.h"

struct argParser {
    int argc;
    char** argv;

    struct argArgument* settings;
    unsigned int settingCount;
    int argcErrOffset;

    struct argArgument* currentArgument;

    Table argumentTable;
    Table shortArgTable;
    Table modes;

    const char* internalArg;

    bool hasError : 1;
    bool canGetArg : 1;
    bool canGetInternalArg : 1;
    bool hasGotArg : 1;
    bool setupCompleted : 1;
    bool isNegated : 1;
    bool isInternalCall : 1;
};

struct argArgument {
    const char* name;
    const char shortName;
    const char* helpMessage;
    void (*callback)(struct argParser*, void* ctx);
    void* callbackCtx;
    bool isOption : 1;
    bool isRequired : 1;
    bool isDone : 1;
    bool isMode : 1;
};

struct stringList {
    ARRAY_DEFINE(const char*, data);
};

// individual element in a hashmap style option
struct argMapElement {
    const char* elementName;

    void (*callback)(struct argParser*, void* ctx);

    void* ctx;
};

// data for parsing an argument like -fopt=true
struct argMapData {
    // null terminated list
    struct argMapElement* args;
    Table map;
};

bool parseArgs(struct argParser*);

void argSet(struct argParser* parser, void* ctx);
void argBool(struct argParser* parser, void* ctx);
void argInt(struct argParser* parser, void* ctx);
void argPush(struct argParser* parser, void* ctx);
void argOneString(struct argParser* parser, void* ctx);
void argMode(struct argParser* parser, void* ctx);
void argAlias(struct argParser* parser, void* ctx);
void argMap(struct argParser* parser, void* ctx);

void argError(struct argParser* parser, const char* message, ...);

const char* argNextString(struct argParser* parser, bool shouldEmitError);
int argNextInt(struct argParser* parser, bool shouldEmitError, bool* didError);

#endif