#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../headers/symtable.h"
#include "../headers/quad.h"
#include "../headers/targetcode.h"

#define hash_multi 65599

static unsigned int SymTable_hash(const char *key) {
    unsigned int hash = 0U;
    size_t i;

    for(i = 0U; key[i] != '\0'; i++) hash = hash * hash_multi + key[i];

    return hash % HASH_SIZE;
}

SymTable *SymTable_New(void) {

    SymTable *table = malloc(sizeof(SymTable));

    assert(table);

    table->length = 0;

    for(int i = 0; i < HASH_SIZE; i++) table->buckets[i] = NULL;
    for(int i = 0; i < MAX_SCOPE; i++) table->scopeLists[i] = NULL;

    return table;
}

void SymTable_Free(SymTable *table) {

    Binding *pCurrent;
    int index;

    assert(table);

    for(index = 0; index < HASH_SIZE; index++) {
        pCurrent = table->buckets[index];

        while(pCurrent) {
            Binding *next = pCurrent->next;
            free((void *)pCurrent->key);
            free((void *)pCurrent->entry->name);
            free((void *)pCurrent->entry);
            free(pCurrent);
            pCurrent = next;
        }
    }

    table->length = 0;
    free(table);
}

unsigned int SymTable_getLength(SymTable *table) {

    assert(table);

    return table->length;
}

SymTableEntry *SymTable_Insert(SymTable *table, const char *name, unsigned int scope, unsigned int line, SymbolType type) {

    SymTableEntry *existing;
    SymTableEntry *entry;
    Binding *pNewBinding;

    assert(table);
    assert(name);

    existing = SymTable_Lookup(table, name, scope);
    if(existing && existing->isActive) return NULL;

    entry = malloc(sizeof(SymTableEntry));
    assert(entry);

    entry->name = strdup(name);
    assert(entry->name);

    entry->scope = scope;
    entry->line = line;
    entry->offset = 0;
    entry->space = 0;
    entry->localCount = 0;
    entry->taddress = 0;
    entry->isActive = 1;
    entry->type = type;
    entry->nextInScope = NULL;

    SymTableEntry **scopeHead = &table->scopeLists[scope];
    if(*scopeHead == NULL || (*scopeHead)->line > line) {
        entry->nextInScope = *scopeHead;
        *scopeHead = entry;
    } else {
        SymTableEntry *curr = *scopeHead;
        while(curr->nextInScope && curr->nextInScope->line <= line) curr = curr->nextInScope;

        entry->nextInScope = curr->nextInScope;
        curr->nextInScope = entry;
    }

    pNewBinding = malloc(sizeof(Binding));
    assert(pNewBinding);

    pNewBinding->key = strdup(name);
    assert(pNewBinding->key);

    pNewBinding->entry = entry;
    pNewBinding->next = NULL;

    unsigned int index = SymTable_hash(name);
    Binding **bucketHead = &table->buckets[index];

    if(*bucketHead == NULL || (*bucketHead)->entry->line > line) {
        pNewBinding->next = *bucketHead;
        *bucketHead = pNewBinding;
    } else {
        Binding *curr = *bucketHead;
        while(curr->next && curr->next->entry->line <= line) curr = curr->next;

        pNewBinding->next = curr->next;
        curr->next = pNewBinding;
    }

    table->length++;
    return entry;
}


SymTableEntry *SymTable_Lookup(SymTable *table, const char *name, unsigned int scope) {

    SymTableEntry *pCurrent;

    assert(table);
    assert(name);

    pCurrent = table->scopeLists[scope];
    while(pCurrent) {
        if(pCurrent->isActive && strcmp(pCurrent->name, name) == 0) return pCurrent;
        pCurrent = pCurrent->nextInScope;
    }
    return NULL;
}

SymTableEntry *SymTable_LookupAny(SymTable *table, const char *name) {

    SymTableEntry *pCurrent;

    assert(table);
    assert(name);

    for(int s = MAX_SCOPE - 1; s >= 0; s--) {
        pCurrent = table->scopeLists[s];
        while(pCurrent) {
            if(pCurrent->isActive && strcmp(pCurrent->name, name) == 0) return pCurrent;
            pCurrent = pCurrent->nextInScope;
        }
    }
    return NULL;
}

void SymTable_Hide(SymTable *table, unsigned int scope) {

    SymTableEntry *pCurrent;

    assert(table);
    if(scope == 0) return;

    pCurrent = table->scopeLists[scope];
    while(pCurrent) {
        pCurrent->isActive = 0;
        pCurrent = pCurrent->nextInScope;
    }
}

SymTable *SymTable_Initialize(void) {
    SymTable* table = SymTable_New();

    const char *libfuncs[] = {
        "print", "input", "objectmemberkeys", "objecttotalmembers", "objectcopy",
        "totalarguments", "argument", "typeof", "strtonum", "sqrt", "cos", "sin"
    };

    for(int i = 0; i < 12; i++) {
        SymTableEntry *entry = SymTable_Insert(table, libfuncs[i], 0, 0, LIBFUNC);
        if(entry) entry->offset = 0;
    }

    return table;
}

void SymTable_Print(SymTable *table) {
    for(int s = 0; s < MAX_SCOPE; s++) {
        SymTableEntry *curr = table->scopeLists[s];
        if(!curr) continue;

        printf("\n---------------  Scope #%d  ---------------\n", s);

        while(curr) {
            const char *typeStr =
                curr->type == GLOBAL_VAR ? "global variable" :
                curr->type == LOCAL_VAR  ? "local variable" :
                curr->type == FORMAL     ? "formal argument" :
                curr->type == USERFUNC   ? "user function" :
                curr->type == LIBFUNC    ? "library function" : "unknown";

            printf("\"%s\" [%s] (line %u) (scope %u) (offset %u)\n",
                   curr->name, typeStr, curr->line, curr->scope, curr->offset);

            curr = curr->nextInScope;
        }
    }
}