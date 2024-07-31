// Class:       CS 4390 - Computer Networks
// Assignment:  Chat Application Project
// Author:      Juan Llamas
// Build:       gcc -std=c99 -lncurses -lm -lpthread *.c -o chat
// File:        protocol.h
// Description: This file contains the definitions for the chat protocol.

#pragma once
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "string.h"

typedef enum {
    FRAME_IDENT = 0,
    FRAME_MSG = 1,
    FRAME_PING = 2,
    FRAME_PONG = 3,
} FrameType;

typedef struct {
    FrameType type;
} Frame;

typedef struct {
    FrameType type;
    String name;
} IdentFrame;

typedef struct {
    FrameType type;
    String content;
    uint8_t attachmentCount;
    String* attachmentNames;
    uint32_t *attachmentSizes;
} MsgFrame;

typedef struct PingFrame_t {
    FrameType type;
    uint32_t lastActive;
} PingFrame;

typedef struct PingFrame_t PongFrame;

Frame* protocol_frame_new(FrameType type);
int protocol_frame_write(int socket, Frame* frame);
int protocol_frame_read(int socket, Frame** frame);
int protocol_frame_write_ident(int socket, IdentFrame* frame);
int protocol_frame_read_ident(int socket, IdentFrame** frame);
int protocol_frame_write_msg(int socket, MsgFrame* frame);
int protocol_frame_read_msg(int socket, MsgFrame** frame);
int protocol_frame_write_ping(int socket, PingFrame* frame);
int protocol_frame_read_ping(int socket, PingFrame** frame);
int protocol_frame_write_pong(int socket, PongFrame* frame);
int protocol_frame_read_pong(int socket, PongFrame** frame);
void protocol_frame_free(Frame* frame);
