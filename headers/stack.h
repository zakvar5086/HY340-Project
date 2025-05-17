#ifndef STACK_H
#define STACK_H

typedef struct Stack Stack;

void *newStack(void);
void push(void *stack, void *item);
void *pop(void *stack);
void *top(void *stack);
void *isEmpty(void *stack);
void *destroyStack(void *stack);
void *printStack(void *stack);

#endif