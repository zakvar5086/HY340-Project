#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../headers/stack.h"
#include "../headers/symtable.h"
#include "../headers/scope_offset_manager.h"

int currentScopeSpace = PROGRAM_SPACE;
int programOffset = 0;
int formalOffset = 0;
int localOffset = 0;
void *scopeOffsetStack = NULL;

void initOffsetManagement() {
    currentScopeSpace = PROGRAM_SPACE;
    programOffset = 0;
    formalOffset = 0;
    localOffset = 0;
    if(!scopeOffsetStack) {
        scopeOffsetStack = newStack();
    }
}

int currscopespace() {
    return currentScopeSpace;
}

int currscopeoffset() {
    switch(currentScopeSpace) {
    case PROGRAM_SPACE:
        return programOffset;
    case FORMAL_SPACE:
        return formalOffset;
    case LOCAL_SPACE:
        return localOffset;
    default:
        return 0;
    }
}

void inccurrscopeoffset() {
    switch(currentScopeSpace) {
    case PROGRAM_SPACE:
        programOffset++;
        break;
    case FORMAL_SPACE:
        formalOffset++;
        break;
    case LOCAL_SPACE:
        localOffset++;
        break;
    default:
        return;
    }
}

void resetformalargsoffset() {
    formalOffset = 0;
}

void resetfunctionlocalsoffset() {
    localOffset = 0;
}

void enterscopespace() {
    currentScopeSpace++;
}

void exitscopespace() {
    currentScopeSpace -= 2;
    if(currentScopeSpace < PROGRAM_SPACE) {
        currentScopeSpace = PROGRAM_SPACE;
    }
}

void push_curr_offset() {
    int *savedOffset = (int *)malloc(sizeof(int));
    *savedOffset = currscopeoffset();
    pushStack(scopeOffsetStack, savedOffset);
}

int pop_and_top() {
    int *savedOffset = (int *)popStack(scopeOffsetStack);
    if(savedOffset) {
        int offset = *savedOffset;
        free(savedOffset);
        return offset;
    }
    return 0;
}

void restorecurrscopeoffset(int offset) {
    switch(currentScopeSpace) {
    case PROGRAM_SPACE:
        programOffset = offset;
        break;
    case FORMAL_SPACE:
        formalOffset = offset;
        break;
    case LOCAL_SPACE:
        localOffset = offset;
        break;
    default:
        return;
    }
}

void assignSymbolSpaceAndOffset(SymTableEntry *sym) {
    if(!sym) return;

    int space;
    if(sym->type == GLOBAL_VAR) {
        space = PROGRAM_SPACE;
    } else if(sym->type == FORMAL) {
        space = FORMAL_SPACE;
    } else if(sym->type == LOCAL_VAR) {
        space = (currentScopeSpace == PROGRAM_SPACE) ? PROGRAM_SPACE : LOCAL_SPACE;
    } else {
        space = currentScopeSpace;
    }

    sym->space = space;
    sym->offset = currscopeoffset();

    inccurrscopeoffset();
}

// Debug function to print current state
void printScopeState() {
    printf("Current Scope Space: %d\n", currentScopeSpace);
    printf("Program Offset: %d\n", programOffset);
    printf("Formal Offset: %d\n", formalOffset);
    printf("Local Offset: %d\n", localOffset);
    printf("Current Offset: %d\n", currscopeoffset());
    printf("---\n");
}


void enterFunctionPrefix() {
    push_curr_offset();

    enterscopespace();
    resetformalargsoffset();
}

void enterFunctionLocals() {
    enterscopespace();
    resetfunctionlocalsoffset();
}

int exitFunctionBody() {
    int totalLocals = currscopeoffset();

    exitscopespace();

    return totalLocals;
}

void exitFunctionDefinition() {
    exitscopespace();

    int oldOffset = pop_and_top();
    restorecurrscopeoffset(oldOffset);
}

SymbolType determineVariableType() {
    if(currentScopeSpace == PROGRAM_SPACE) {
        return GLOBAL_VAR;
    } else if(currentScopeSpace == FORMAL_SPACE) {
        return FORMAL;
    } else {
        return LOCAL_VAR;
    }
}

SymTableEntry *declareVariable(SymTable *symTable, const char *name,
                               unsigned int scope, unsigned int line) {
    SymbolType varType = determineVariableType();
    SymTableEntry *sym = SymTable_Insert(symTable, name, scope, line, varType);

    if(sym) {
        assignSymbolSpaceAndOffset(sym);
    }

    return sym;
}

void enterBlock() {
    if(currscopespace() == PROGRAM_SPACE) {
        int *savedOffset = malloc(sizeof(int));
        *savedOffset = currscopeoffset();
        pushStack(scopeOffsetStack, savedOffset);

        restorecurrscopeoffset(0);
    }
}

void exitBlock(int isFunctionBlock) {
    if(currscopespace() == PROGRAM_SPACE && !isFunctionBlock) {
        int *savedOffset = popStack(scopeOffsetStack);
        if(savedOffset) {
            restorecurrscopeoffset(*savedOffset);
            free(savedOffset);
        }
    }
}