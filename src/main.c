// Class:       CS 4390 - Computer Networks
// Assignment:  Chat Application Project
// Author:      Juan Llamas
// Build:       gcc -std=c99 -lncurses -lm -lpthread *.c -o chat
// File:        protocol.h
// Description: This file contains the main entry point for the chat application,
//              as well as the argument parsing logic.

#include <stdbool.h>
#include <argp.h>
#include <stdlib.h>
#include "string.h"
#include "app.h"

const char *argp_program_version = "MyChat 0.1.0";
const char *argp_program_bug_address = "<jll210001@utdallas.edu>";
static char doc[] = "A simple chat application.";
static char args_doc[] = "";
static struct argp_option options[] = { 
    { "address", 'a', "ADDRESS", 0, "Address to connect to" },
    { "port", 'p', "PORT", 0, "Port to connect to" },
    { "server", 's', 0, 0, "Run as server" },
    { "name", 'n', "NAME", 0, "Name to use" },
    { 0 }
};

typedef struct {
    char *address;
    int port;
    bool server;
    char *name;
} Args;

static error_t parse_opt(int key, char* arg, struct argp_state *state) {
    Args *args = state->input;
    switch (key) {
        case 'a':
            args->address = arg;
            break;
        case 'p':
            args->port = atoi(arg);
            break;
        case 's':
            args->server = true;
            break;
        case 'n':
            args->name = arg;
            break;
        case ARGP_KEY_ARG:
            return 0;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char** argv) {
    int result;

    char* username = getenv("USER");
    Args args = {
        .address = "127.0.0.1",
        .port = 0,
        .server = false,
        .name = username == NULL ? "Unknown" : username
    };

    if ((result = argp_parse(&argp, argc, argv, 0, 0, &args)) != 0)
        return result;

    if (args.port == 0) {
        fprintf(stderr, "Port is required\n");
        return 1;
    }

    ChatApp* app = malloc(sizeof(ChatApp));
    chat_app_init(app, string_new_static(args.name), args.server);
    chat_app_render(app);
    if (result = chat_app_connect(app, args.address, args.port) != 0) {
        chat_app_destroy(app);
        chat_app_free(app);
        fprintf(stderr, "Failed to connect\n");
        return result;
    }
    result = chat_app_run(app);
    chat_app_destroy(app);
    chat_app_free(app);
    return result;
}
