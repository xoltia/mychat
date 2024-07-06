#pragma once
#include <pthread.h>
#include "protocol.h"

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    Frame* frames;
    int frameCount;
    int frameAllocated;
} FrameQueue;

FrameQueue* frame_queue_new(int initialSize);
void frame_queue_lock(FrameQueue* queue);
void frame_queue_unlock(FrameQueue* queue);
void frame_queue_push(FrameQueue* queue, Frame* frame);
Frame* frame_queue_peek(FrameQueue* queue);
Frame* frame_queue_pop(FrameQueue* queue);
bool frame_queue_is_empty(FrameQueue* queue);
void frame_queue_wait_and_lock(FrameQueue* queue);
void frame_queue_free(FrameQueue* queue);
void frame_queue_recv_loop(FrameQueue* queue, int socket, void (*callback)(Frame*));
void frame_queue_send_loop(FrameQueue* queue, int socket, void (*callback)(Frame*));
