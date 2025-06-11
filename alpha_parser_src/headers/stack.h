#ifndef STACK_H
#define STACK_H

typedef struct Stack Stack;

void *newStack(void);
void pushStack(void *stack, void *item);
void *popStack(void *stack);
void *topStack(void *stack);
void *isEmptyStack(void *stack);
void *destroyStack(void *stack);
void *printStack(void *stack);

#endif