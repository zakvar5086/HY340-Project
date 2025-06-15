#ifndef SCOPE_OFFSET_MANAGER_H
#define SCOPE_OFFSET_MANAGER_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "stack.h"
#include "symtable.h"

typedef enum {
    PROGRAM_SPACE = 1,
    FORMAL_SPACE = 2,
    LOCAL_SPACE = 3
} ScopeSpace;

extern int currentScopeSpace;
extern int programOffset;
extern int formalOffset;
extern int localOffset;
extern void *scopeOffsetStack;

void initOffsetManagement();
int currscopespace();
int currscopeoffset();
void inccurrscopeoffset();
void resetformalargsoffset();
void resetfunctionlocalsoffset();
void enterscopespace();
void exitscopespace();
void push_curr_offset();
int pop_and_top();
void restorecurrscopeoffset(int offset);
void assignSymbolSpaceAndOffset(SymTableEntry *sym);
void printScopeState();
void enterFunctionPrefix();
void enterFunctionLocals();
int exitFunctionBody();
void exitFunctionDefinition();
SymbolType determineVariableType();
SymTableEntry *declareVariable(SymTable *symTable, const char *name,
                               unsigned int scope, unsigned int line);
void enterBlock();
void exitBlock(int isFunctionBlock);

#endif