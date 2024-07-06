#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include "queue.h"

FrameQueue* frame_queue_new(int initialSize) {
    FrameQueue* queue = (FrameQueue*)malloc(sizeof(FrameQueue));
    queue->frames = (Frame*)malloc(sizeof(Frame) * initialSize);
    queue->frameCount = 0;
    queue->frameAllocated = initialSize;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}

void frame_queue_lock(FrameQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
}

void frame_queue_unlock(FrameQueue* queue) {
    pthread_mutex_unlock(&queue->mutex);
}

void frame_queue_push(FrameQueue* queue, Frame* frame) {
    if (queue->frameCount == queue->frameAllocated) {
        queue->frameAllocated *= 2;
        queue->frames = (Frame*)realloc(queue->frames, sizeof(Frame) * queue->frameAllocated);
    }
    queue->frames[queue->frameCount++] = *frame;
    pthread_cond_signal(&queue->cond);
}

bool frame_queue_is_empty(FrameQueue* queue) {
    return queue->frameCount == 0;
}

void frame_queue_wait_and_lock(FrameQueue* queue) {
    frame_queue_lock(queue);
    while (frame_queue_is_empty(queue)) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
}

Frame* frame_queue_peek(FrameQueue* queue) {
    if (frame_queue_is_empty(queue))
        return NULL;
    return &queue->frames[0];
}

Frame* frame_queue_pop(FrameQueue* queue) {
    Frame* frame = &queue->frames[0];
    for (int i = 0; i < queue->frameCount - 1; i++) {
        queue->frames[i] = queue->frames[i + 1];
    }
    queue->frameCount--;
    return frame;
}

void frame_queue_free(FrameQueue* queue) {
    free(queue->frames);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    free(queue);
}

void frame_queue_recv_loop(FrameQueue* queue, int socket, void (*callback)(Frame*)) {
    while (1) {
        Frame* frame = protocol_frame_new(FRAME_IDENT);
        if (protocol_frame_read(socket, &frame) < 0) {
            // TODO: Handle read errors
            protocol_frame_free(frame);
            continue;
        }

        if (callback != NULL)
            callback(frame);
    
        frame_queue_lock(queue);
        frame_queue_push(queue, frame);
        frame_queue_unlock(queue);
    }
}

void frame_queue_send_loop(FrameQueue* queue, int socket, void (*callback)(Frame*)) {
    while (1) {
        frame_queue_wait_and_lock(queue);
        Frame* frame = frame_queue_pop(queue);
        // TODO: Handle write errors
        protocol_frame_write(socket, frame);
        if (callback != NULL)
            callback(frame);
        frame_queue_unlock(queue);
        protocol_frame_free(frame);
    }
}