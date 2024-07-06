#pragma once
#include <stdint.h>
#include <pthread.h>
#include <ncurses.h>

#define IDLE_CHECK_INTERVAL 1
#define IDLE_TIMEOUT 10
#define PING_INTERVAL 2

typedef struct {
    bool isOutgoing;
    String content;
    String* attachments;
    int attachmentCount;
} Message;

void message_free(Message* message);

typedef struct {
    String name;
    String peerName;
    String peerAddr;
    Message** messages;
    int messageCount;
    enum {
        DISCONNECTED,
        CONNECTED,
        IDLE,
    } status;
    String sendBuffer;
    WINDOW* statusWindow;
    WINDOW* messageWindow;
    WINDOW* inputWindow;
    pthread_mutex_t stateMutex;
    pthread_mutex_t sendMutex;
    int socketfd;
    uint32_t lastActive;
    uint32_t peerLastActive;
    bool isServer;
} ChatApp;

void chat_app_init(ChatApp* app, String name, bool isServer);
int chat_app_connect(ChatApp* app, char* address, int port);
int chat_app_run(ChatApp* app);
void chat_app_render(ChatApp* app);
void chat_app_destroy(ChatApp* app);
void chat_app_free(ChatApp* app);
