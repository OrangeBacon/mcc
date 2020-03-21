#include "token.h"

#define STRING_TOKEN(x) #x,
static const char* TokenNames[] = {
    FOREACH_TOKEN(STRING_TOKEN)
};
#undef STRING_TOKEN

const char* TokenTypeToString(TokenType type) {
    return TokenNames[type];
}