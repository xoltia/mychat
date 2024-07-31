// Class:       CS 4390 - Computer Networks
// Assignment:  Chat Application Project
// Author:      Juan Llamas
// Build:       gcc -std=c99 -lncurses -lm -lpthread *.c -o chat
// File:        string.h
// Description: This file contains the type definitions for the String
//              utility class.

#pragma once

typedef struct {
    char* data;
    int length;
    int allocated;
} String;

String string_new(int initialSize);
String string_new_static(char* data);
void string_init(String* string, int initialSize);
void string_init_static(String* string, char* data);
void string_grow(String* string, int additionalSize);
void string_append_static(String* string, char* data);
void string_append(String* string, String* other);
void string_append_char(String* string, char c);
void string_free(String* string);
void string_pop_char(String* string);
void string_clear(String* string);
String string_copy(String* string);
