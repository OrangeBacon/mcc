#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>
#include <stdint.h>

typedef enum streamError {
    STREAM_NO_ERROR,
    STREAM_OUT_OF_DATA,
    STREAM_NO_PREDECESSOR,
} streamError;

struct stream {
    struct streamFn* functions;

    size_t count;
    size_t currentDepth;

    void* data;
    size_t len;
};

#define STREAM_LAYER \
    struct stream* stream, \
    void** data, \
    size_t* len, \
    streamError (*getNextArg)(struct stream*, void** value), \
    void* ctx

struct streamFn {
    streamError (*fn)(STREAM_LAYER);
    size_t bufferTypeSize;
    void* ctx;
    void* buffer;
    size_t bufferLen;
};

streamError streamRun(struct stream* stream, void* start, size_t len);

#endif