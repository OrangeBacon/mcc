#include "stream.h"

static streamError streamGetNext(struct stream* stream, void** data);
static streamError streamInvoke(struct stream* stream) {
    size_t count = stream->currentDepth;
    return stream->functions[count].fn(
        stream,
        &stream->functions[count].buffer,
        &stream->functions[count].bufferLen,
        streamGetNext,
        stream->functions[count].ctx);
}

static streamError streamGetNext(struct stream* stream, void** data) {
    if(stream->currentDepth < 1) {
        return STREAM_NO_PREDECESSOR;
    }

    struct streamFn *fn = &stream->functions[stream->currentDepth-1];
    if(fn->bufferLen == 0) {
        stream->currentDepth--;
        streamError err = streamInvoke(stream);
        stream->currentDepth++;
        if(err != STREAM_NO_ERROR) return err;
    }

    fn->bufferLen--;
    *data = fn->buffer;
    fn->buffer = (char*)fn->buffer + fn->bufferTypeSize;
    return STREAM_NO_ERROR;
}

streamError streamRun(struct stream* stream, void* start, size_t len) {
    stream->data = start;
    stream->len = len;

    if(stream->count == 0 && stream->functions[0].fn != NULL) {
        size_t count = 0;
        struct streamFn* fn = &stream->functions[0];

        while(fn->fn != NULL) {
            count++;
            fn++;
        }

        stream->count = count;
    }

    stream->currentDepth = stream->count - 1;
    return streamInvoke(stream);
}