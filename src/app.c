#define _GNU_SOURCE 1
#include <stdio.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <argp.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "string.h"
#include "protocol.h"
#include "app.h"

void message_free(Message* message) {
    string_free(&message->content);
    for (int i = 0; i < message->attachmentCount; i++)
        string_free(&message->attachments[i]);
    free(message->attachments);
    free(message);
}

void chat_app_destroy(ChatApp* app) {
    delwin(app->statusWindow);
    delwin(app->messageWindow);
    delwin(app->inputWindow);
    endwin();

    if (app->socketfd >= 0)
        close(app->socketfd);
}

void chat_app_free(ChatApp* app) {
    string_free(&app->name);
    string_free(&app->peerName);
    string_free(&app->peerAddr);
    string_free(&app->sendBuffer);
    for (int i = 0; i < app->messageCount; i++)
        message_free(app->messages[i]);
    free(app->messages);
    free(app);
}

void chat_app_init(ChatApp* app, String name, bool isServer) {
    initscr();

    start_color();
    // status text green background
    init_pair(1, COLOR_BLACK, COLOR_GREEN);
    // status text red background
    init_pair(2, COLOR_BLACK, COLOR_RED);
    // status text yellow background
    init_pair(3, COLOR_BLACK, COLOR_YELLOW);

    app->name = name;
    app->peerName = string_new_static("");
    app->peerAddr = string_new_static("");
    app->status = DISCONNECTED;
    app->messages = NULL;
    app->messageCount = 0;
    app->statusWindow = newwin(1, 0, 0, 0);
    app->messageWindow = newwin(0, 0, 1, 0);
    app->inputWindow = newwin(1, 0, LINES - 1, 0);
    app->socketfd = -1;
    app->isServer = isServer;
    app->peerLastActive = 0;
    app->lastActive = time(NULL);
    string_init(&app->sendBuffer, 0);
    pthread_mutex_init(&app->stateMutex, NULL);
    pthread_mutex_init(&app->sendMutex, NULL);

    scrollok(app->messageWindow, TRUE);
    wrefresh(app->statusWindow);
    wrefresh(app->messageWindow);
    wrefresh(app->inputWindow);
}

void chat_app_render(ChatApp* app) {
    // Prevent state changes while rendering
    pthread_mutex_lock(&app->stateMutex);

    werase(app->statusWindow);
    werase(app->messageWindow);
    werase(app->inputWindow);
    switch (app->status) {
        case DISCONNECTED:
            wbkgd(app->statusWindow, COLOR_PAIR(2));
            break;
        case CONNECTED:
            wbkgd(app->statusWindow, COLOR_PAIR(1));
            break;
        case IDLE:
            wbkgd(app->statusWindow, COLOR_PAIR(3));
            break;
    }

    char* statusString = app->status == DISCONNECTED
        ? "Disconnected"
        : app->status == CONNECTED
        ? "Connected" 
        : "Idle";

    int padding = COLS - strlen(statusString) - app->peerAddr.length;
    wprintw(app->statusWindow, "%s", statusString);
    for (int i = 0; i < padding; i++) wprintw(app->statusWindow, " ");
    wprintw(app->statusWindow, "%s\n", app->peerAddr.data);
    for (int i = 0; i < app->messageCount; i++) {
        Message* message = app->messages[i];
        wprintw(app->messageWindow, "%s: %s\n", message->isOutgoing ? app->name.data : app->peerName.data, message->content.data);
        for (int j = 0; j < message->attachmentCount; j++)
            wprintw(app->messageWindow, "Attachment: %s\n", message->attachments[j].data);
    }

    if (app->sendBuffer.length > COLS - 2) {
        wprintw(app->inputWindow, "..%s", app->sendBuffer.data + app->sendBuffer.length - (COLS - 4));
    } else {
        wprintw(app->inputWindow, "> %s", app->sendBuffer.data);
    }
    

    wrefresh(app->statusWindow);
    wrefresh(app->messageWindow);
    wrefresh(app->inputWindow);

    pthread_mutex_unlock(&app->stateMutex);
}

void chat_app_append_message(ChatApp* app, Message* message) {
    pthread_mutex_lock(&app->stateMutex);
    if (app->messageCount > 0)
        app->messages = realloc(app->messages, app->messageCount * 2 * sizeof(Message*));
    else
        app->messages = malloc(sizeof(Message*));
    app->messages[app->messageCount++] = message;
    pthread_mutex_unlock(&app->stateMutex);
}

void chat_app_send_message_buffer(ChatApp* app) {
    pthread_mutex_lock(&app->sendMutex);
    MsgFrame* frame = (MsgFrame*)protocol_frame_new(FRAME_MSG);
    frame->content = string_copy(&app->sendBuffer);
    frame->attachmentCount = 0;
    frame->attachmentNames = NULL;
    frame->attachmentSizes = NULL;
    protocol_frame_write_msg(app->socketfd, frame);
    protocol_frame_free((Frame*)frame);
    pthread_mutex_unlock(&app->sendMutex);

    Message* message = malloc(sizeof(Message));
    message->isOutgoing = true;
    message->content = string_copy(&app->sendBuffer);
    message->attachments = NULL;
    message->attachmentCount = 0;
    chat_app_append_message(app, message);
    string_clear(&app->sendBuffer);
}

void chat_app_recv_loop(ChatApp* app) {
    while (true) {
        Frame* frame = protocol_frame_new(FRAME_IDENT);
        if (protocol_frame_read(app->socketfd, &frame) < 0) {
            protocol_frame_free(frame);
            chat_app_destroy(app);
            chat_app_free(app);
            fprintf(stderr, "Connection closed\n");
            exit(1);
        }

        switch (frame->type) {
            case FRAME_IDENT: {
                IdentFrame* identFrame = (IdentFrame*)frame;
                pthread_mutex_lock(&app->stateMutex);
                string_free(&app->peerName);
                app->peerName = string_copy(&identFrame->name);
                app->status = CONNECTED;
                pthread_mutex_unlock(&app->stateMutex);
                
                if (app->isServer) {
                    pthread_mutex_lock(&app->sendMutex);
                    IdentFrame* responseIdentFrame = (IdentFrame*)protocol_frame_new(FRAME_IDENT);
                    responseIdentFrame->name = string_copy(&app->name);
                    protocol_frame_write_ident(app->socketfd, responseIdentFrame);
                    protocol_frame_free((Frame*)responseIdentFrame);
                    pthread_mutex_unlock(&app->sendMutex);
                }

                // User is connected, render the UI
                chat_app_render(app);
                break;
            }
            case FRAME_MSG: {
                MsgFrame* msgFrame = (MsgFrame*)frame;
                Message* message = malloc(sizeof(Message));
                message->isOutgoing = false;
                message->content = string_copy(&msgFrame->content);
                message->attachments = malloc(sizeof(String) * msgFrame->attachmentCount);
                message->attachmentCount = msgFrame->attachmentCount;
                for (int i = 0; i < msgFrame->attachmentCount; i++) {
                    message->attachments[i] = string_new(0);
                }
                chat_app_append_message(app, message);
                // Message received, render the UI
                chat_app_render(app);
                break;
            }
            case FRAME_PING: {
                PingFrame* pingFrame = (PingFrame*)frame;
                pthread_mutex_lock(&app->stateMutex);
                app->peerLastActive = pingFrame->lastActive;
                pthread_mutex_unlock(&app->stateMutex);

                pthread_mutex_lock(&app->sendMutex);
                PongFrame* pongFrame = (PongFrame*)protocol_frame_new(FRAME_PONG);
                // TODO: Use correct timestamp
                pongFrame->lastActive = app->lastActive;
                protocol_frame_write_pong(app->socketfd, pongFrame);
                protocol_frame_free((Frame*)pongFrame);
                pthread_mutex_unlock(&app->sendMutex);
                break;
            }
            case FRAME_PONG: {
                PongFrame* pongFrame = (PongFrame*)frame;
                pthread_mutex_lock(&app->stateMutex);
                app->peerLastActive = pongFrame->lastActive;
                pthread_mutex_unlock(&app->stateMutex);
                break;
            }
        }

        protocol_frame_free(frame);
    }
}

void chat_app_check_idle_loop(ChatApp* app) {
    while (true) {
        sleep(IDLE_CHECK_INTERVAL);
        bool change;
        if (time(NULL) - app->peerLastActive > IDLE_TIMEOUT) {
            pthread_mutex_lock(&app->stateMutex);
            change = app->status == CONNECTED;
            if (change)
                app->status = IDLE;
            pthread_mutex_unlock(&app->stateMutex);
        } else {
            pthread_mutex_lock(&app->stateMutex);
            change = app->status == IDLE;
            if (change)
                app->status = CONNECTED;
            pthread_mutex_unlock(&app->stateMutex);
        }

        if (change) {
            chat_app_render(app);
        }
    }
}

int chat_app_connect_server(ChatApp* app, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {
            .s_addr = INADDR_ANY
        }
    };

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(sockfd, 1) < 0) {
        perror("listen");
        return 1;
    }

    struct sockaddr_in clientAddr;
    socklen_t clientAddrSize = sizeof(clientAddr);
    app->socketfd = accept(sockfd, (struct sockaddr *)&clientAddr, &clientAddrSize);
    if (app->socketfd < 0) {
        perror("accept");
        return 1;
    }

    char* portString;
    char* clientAddrString = inet_ntoa(clientAddr.sin_addr);
    string_append_static(&app->peerAddr, clientAddrString);
    string_append_char(&app->peerAddr, ':');
    if (asprintf(&portString, "%hu", port) != -1) {
        string_append_static(&app->peerAddr, portString);
        free(portString);
    }

    return 0;
}

int chat_app_connect_client(ChatApp* app, char* address, uint16_t port) {
    char* portString;
    string_append_static(&app->peerAddr, address);
    string_append_char(&app->peerAddr, ':');
    if (asprintf(&portString, "%hu", port) != -1) {
        string_append_static(&app->peerAddr, portString);
        free(portString);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port)
    };

    if (inet_pton(AF_INET, address, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    app->socketfd = sockfd;

    IdentFrame* identFrame = (IdentFrame*)protocol_frame_new(FRAME_IDENT);
    identFrame->name = app->name;
    protocol_frame_write_ident(app->socketfd, identFrame);
    protocol_frame_free((Frame*)identFrame);
    return 0;
}

int chat_app_connect(ChatApp* app, char* address, uint16_t port) {
    return app->isServer
        ? chat_app_connect_server(app, port)
        : chat_app_connect_client(app, address, port);
}

void chat_app_ping_loop(ChatApp* app) {
    while (true) {
        pthread_mutex_lock(&app->sendMutex);
        PingFrame* frame = (PingFrame*)protocol_frame_new(FRAME_PING);
        // TODO: Use correct timestamp
        frame->lastActive = app->lastActive;
        protocol_frame_write_ping(app->socketfd, frame);
        protocol_frame_free((Frame*)frame);
        pthread_mutex_unlock(&app->sendMutex);
        sleep(PING_INTERVAL);
    }
}

int chat_app_run(ChatApp* app) {
    pthread_t recvThread, checkIdleThread, pingThread;
    if (pthread_create(&recvThread, NULL, (void* (*)(void*))chat_app_recv_loop, app) != 0) {
        perror("pthread_create");
        return 1;
    }

    if (pthread_create(&checkIdleThread, NULL,  (void* (*)(void*))chat_app_check_idle_loop, app) != 0) {
        perror("pthread_create");
        return 1;
    }

    if (app->isServer) {
        if (pthread_create(&pingThread, NULL, (void* (*)(void*))chat_app_ping_loop, app) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    // Main loop
    while (true) {
        chat_app_render(app);
        int ch = wgetch(app->inputWindow);
        app->lastActive = time(NULL);
        if (ch == 27) // ESC
            break;
        else if (ch == 10) { // ENTER
            if (app->status == DISCONNECTED)
                continue;
            chat_app_send_message_buffer(app);
        } else if (ch == 127) { // BACKSPACE
            if (app->sendBuffer.length > 0)
                string_pop_char(&app->sendBuffer);
        } else {
            string_append_char(&app->sendBuffer, ch);
        }
    }

    pthread_cancel(recvThread);
    pthread_cancel(checkIdleThread);
    if (app->isServer)
        pthread_cancel(pingThread);
}
