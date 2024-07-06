#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "string.h"
#include "protocol.h"

static int read_uint32(int socket, uint32_t* value) {
    uint32_t raw;
    if (read(socket, &raw, 4) != 4)
        return -1;
    *value = ntohl(raw);
    return 0;
}

static int write_uint32(int socket, uint32_t value) {
    uint32_t raw = htonl(value);
    if (write(socket, &raw, 4) != 4)
        return -1;
    return 0;
}

static int write_uint16(int socket, uint16_t value) {
    uint16_t raw = htons(value);
    if (write(socket, &raw, 2) != 2)
        return -1;
    return 0;
}

static int read_uint16(int socket, uint16_t* value) {
    uint16_t raw;
    if (read(socket, &raw, 2) != 2)
        return -1;
    *value = ntohs(raw);
    return 0;
}

Frame* protocol_frame_new(FrameType type) {
    Frame* frame;
    switch (type) {
        case FRAME_IDENT:
            frame = malloc(sizeof(IdentFrame));
            frame->type = FRAME_IDENT;
            break;
        case FRAME_MSG:
            frame = malloc(sizeof(MsgFrame));
            frame->type = FRAME_MSG;
            break;
        case FRAME_PING:
            frame = malloc(sizeof(PingFrame));
            frame->type = FRAME_PING;
            break;
        case FRAME_PONG:
            frame = malloc(sizeof(PongFrame));
            frame->type = FRAME_PONG;
            break;
        default:
            return NULL;
    }
    return frame;
}

int protocol_frame_write(int socket, Frame* frame) {
    switch (frame->type) {
        case FRAME_IDENT:
            return protocol_frame_write_ident(socket, (IdentFrame*)frame);
        case FRAME_MSG:
            return protocol_frame_write_msg(socket, (MsgFrame*)frame);
        case FRAME_PING:
            return protocol_frame_write_ping(socket, (PingFrame*)frame);
        case FRAME_PONG:
            return protocol_frame_write_pong(socket, (PongFrame*)frame);
        default:
            return -1;
    }
}

int protocol_frame_read(int socket, Frame** frame) {
    uint8_t type;
    if (read(socket, &type, 1) != 1)
        return -1;
    switch (type) {
        case FRAME_IDENT:
            return protocol_frame_read_ident(socket, (IdentFrame**)frame);
        case FRAME_MSG: 
            return protocol_frame_read_msg(socket, (MsgFrame**)frame);
        case FRAME_PING:
            return protocol_frame_read_ping(socket, (PingFrame**)frame);
        case FRAME_PONG:
            return protocol_frame_read_pong(socket, (PongFrame**)frame);
        default:
            return -1;
    }
}

/*
* Ident frame format:
* 1 byte: frame type (0)
* 1 byte: name length
* name length bytes: name
*/
int protocol_frame_write_ident(int socket, IdentFrame* frame) {
    uint8_t type = FRAME_IDENT;
    if (write(socket, &type, 1) != 1)
        return -1;
    uint8_t nameLength = frame->name.length;
    if (write(socket, &nameLength, 1) != 1)
        return -1;
    if (write(socket, frame->name.data, nameLength) != nameLength)
        return -1;
    return 0;
}

int protocol_frame_read_ident(int socket, IdentFrame** frame) {
    uint8_t nameLength;
    if (read(socket, &nameLength, 1) != 1)
        return -1;
    *frame = malloc(sizeof(IdentFrame));
    (*frame)->type = FRAME_IDENT;
    (*frame)->name = string_new(nameLength);
    if (read(socket, (*frame)->name.data, nameLength) != nameLength)
        return -1;
    (*frame)->name.length = nameLength;
    return 0;
}

/*
* Msg frame format:
* 1 byte: frame type (1)
* 2 bytes: content length
* 1 byte: attachment count
* for each attachment:
* 1 byte: attachment name length
* attachment name length bytes: attachment name
* 4 bytes: attachment size
* content length bytes: content
*/
int protocol_frame_write_msg(int socket, MsgFrame* frame) {
    uint8_t type = FRAME_MSG;
    if (write(socket, &type, 1) != 1)
        return -1;
    uint16_t contentLength = frame->content.length;
    int result;
    if ((result = write_uint16(socket, contentLength)) < 0)
        return result;
    uint8_t attachmentCount = frame->attachmentCount;
    if (write(socket, &attachmentCount, 1) != 1)
        return -1;
    for (uint8_t i = 0; i < attachmentCount; i++) {
        uint8_t attachmentNameLength = frame->attachmentNames[i].length;
        if (write(socket, &attachmentNameLength, 1) != 1)
            return -1;
        if (write(socket, frame->attachmentNames[i].data, attachmentNameLength) != attachmentNameLength)
            return -1;
        if ((result = write_uint32(socket, frame->attachmentSizes[i])) < 0)
            return result;
    }
    if (write(socket, frame->content.data, contentLength) != contentLength)
        return -1;
    return 0;
}

#include <stdio.h>

int protocol_frame_read_msg(int socket, MsgFrame** frame) {
    uint16_t contentLength;
    int result;
    if ((result = read_uint16(socket, &contentLength)) < 0)
        return result;
    uint8_t attachmentCount;
    if (read(socket, &attachmentCount, 1) != 1)
        return -1;
    *frame = malloc(sizeof(MsgFrame));
    (*frame)->type = FRAME_MSG;
    (*frame)->content = string_new(contentLength);
    (*frame)->attachmentCount = attachmentCount;
    (*frame)->attachmentNames = malloc(sizeof(String) * attachmentCount);
    (*frame)->attachmentSizes = malloc(sizeof(uint32_t) * attachmentCount);
    for (uint8_t i = 0; i < attachmentCount; i++) {
        uint8_t attachmentNameLength;
        if (read(socket, &attachmentNameLength, 1) != 1)
            return -1;
        (*frame)->attachmentNames[i] = string_new(attachmentNameLength);
        if (read(socket, (*frame)->attachmentNames[i].data, attachmentNameLength) != attachmentNameLength)
            return -1;
        if ((result = read_uint32(socket, &(*frame)->attachmentSizes[i])) < 0)
            return result;
    }
    if (read(socket, (*frame)->content.data, contentLength) != contentLength)
        return -1;
    (*frame)->content.data[contentLength] = '\0';
    (*frame)->content.length = contentLength;
    return 0;
}

/*
* Ping frame format:
* 1 byte: frame type (2)
* 4 bytes: last active timestamp
*/
int protocol_frame_write_ping(int socket, PingFrame* frame) {
    uint8_t type = FRAME_PING;
    if (write(socket, &type, 1) != 1)
        return -1;
    return write_uint32(socket, frame->lastActive);
}

int protocol_frame_read_ping(int socket, PingFrame** frame) {
    *frame = malloc(sizeof(PingFrame));
    (*frame)->type = FRAME_PING;
    return read_uint32(socket, &(*frame)->lastActive);
}

/*
* Pong frame format:
* 1 byte: frame type (3)
* 4 bytes: last active timestamp
*/
int protocol_frame_write_pong(int socket, PongFrame* frame) {
    uint8_t type = FRAME_PONG;
    if (write(socket, &type, 1) != 1)
        return -1;
    return write_uint32(socket, frame->lastActive);
}

int protocol_frame_read_pong(int socket, PongFrame** frame) {
    *frame = malloc(sizeof(PongFrame));
    (*frame)->type = FRAME_PONG;
    return read_uint32(socket, &(*frame)->lastActive);
}

void protocol_frame_free(Frame* frame) {
    switch (frame->type) {
        case FRAME_IDENT:
            string_free(&((IdentFrame*)frame)->name);
            break;
        case FRAME_MSG:
            MsgFrame* msgFrame = (MsgFrame*)frame;
            string_free(&msgFrame->content);
            for (uint8_t i = 0; i < msgFrame->attachmentCount; i++) {
                string_free(&msgFrame->attachmentNames[i]);
            }
            free(msgFrame->attachmentNames);
            free(msgFrame->attachmentSizes);
            break;
        case FRAME_PING:
        case FRAME_PONG:
            break;
    }
    free(frame);
}
