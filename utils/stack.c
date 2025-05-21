#include "../headers/stack.h"
#include <stdio.h>
#include <stdlib.h>

#define INITIAL_CAPACITY 16

struct Stack {
    void **data;
    size_t size;
    size_t capacity;
};

void *newStack(void) {
    struct Stack *s = malloc(sizeof *s);
    if(!s) return NULL;
    s->data = malloc(sizeof(void*) * INITIAL_CAPACITY);
    if(!s->data) {
        free(s);
        return NULL;
    }
    s->size = 0;
    s->capacity = INITIAL_CAPACITY;
    return s;
}

void pushStack(void *stack, void *item) {
    struct Stack *s = stack;
    if(!s) return;
    if(s->size == s->capacity) {
        size_t newcap = s->capacity * 2;
        void **tmp = realloc(s->data, sizeof(void*) * newcap);
        if(!tmp) return;
        s->data = tmp;
        s->capacity = newcap;
    }
    s->data[s->size++] = item;
}

void *popStack(void *stack) {
    struct Stack *s = stack;
    if(!s || s->size == 0) return NULL;
    return s->data[--s->size];
}

void *topStack(void *stack) {
    struct Stack *s = stack;
    if(!s || s->size == 0) return NULL;
    return s->data[s->size - 1];
}

void *isEmptyStack(void *stack) {
    struct Stack *s = stack;
    if(!s) return (void*)1;
    return (void*)(size_t)(s->size == 0);
}

void *destroyStack(void *stack) {
    struct Stack *s = stack;
    if(!s) return NULL;
    free(s->data);
    free(s);
    return NULL;
}

void *printStack(void *stack) {
    struct Stack *s = stack;
    if(!s) {
        printf("<null stack>\n");
        return NULL;
    }
    printf("Stack (size = %zu):\n", s->size);
    for(size_t i = s->size; i-- > 0; ) {
        printf("  [%zu] %p\n", i, s->data[i]);
    }
    return s;
}
