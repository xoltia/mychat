#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "string.h"

String string_new(int initialSize) {
    String string;
    string_init(&string, initialSize);
    return string;
}

String string_new_static(char* data) {
    String string;
    string_init_static(&string, data);
    return string;
}

void string_init(String* string, int initialSize) {
    string->length = 0;
    string->allocated = (initialSize > 0 ? initialSize : 0) + 1;
    string->data = malloc(string->allocated);
    string->data[0] = '\0';
}

void string_init_static(String* string, char* data) {
    string->data = data;
    string->length = strlen(data);
    string->allocated = 0;
}

void string_grow(String* string, int additionalSize) {
    int newSize = string->length + additionalSize + 1;
    if (newSize > string->allocated) {
        int oldSize = string->allocated;
        string->allocated = (int)pow(2, ceil(log2(newSize)));
        if (oldSize > 0)
            string->data = realloc(string->data, string->allocated);
        else
            string->data = malloc(string->allocated);
    }
}

void string_append_static(String* string, char* data) {
    int length = strlen(data);
    string_grow(string, length);
    memcpy(string->data + string->length, data, length);
    string->length += length;
    string->data[string->length] = '\0';
}

void string_append(String* string, String* other) {
    string_grow(string, other->length);
    memcpy(string->data + string->length, other->data, other->length);
    string->length += other->length;
    string->data[string->length] = '\0';
}

void string_append_char(String* string, char c) {
    string_grow(string, 1);
    string->data[string->length] = c;
    string->length++;
    string->data[string->length] = '\0';
}

void string_free(String* string) {
    if (string->allocated > 0) {
        free(string->data);
    }
}

void string_pop_char(String* string) {
    if (string->length > 0) {
        string->length--;
        string->data[string->length] = '\0';
    }
}

void string_clear(String* string) {
    string->length = 0;
    string->data[0] = '\0';
}

String string_copy(String* string) {
    String copy = string_new(string->length);
    string_append(&copy, string);
    return copy;
}

