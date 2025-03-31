#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "symtable.h"

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

    for(index = 0; index < HASH_SIZE; index++){
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

SymbolTableEntry *SymTable_Insert(SymTable *table, const char *name, unsigned int scope, unsigned int line, SymbolType type) {
    
    SymbolTableEntry *existing;
    SymbolTableEntry *entry;
    Binding *pNewBinding;
    
    assert(table);
    assert(name);

    existing = SymTable_Lookup(table, name, scope);
    if(existing && existing->isActive) return NULL;

    entry = malloc(sizeof(SymbolTableEntry));
    assert(entry);

    entry->name = strdup(name);
    assert(entry->name);

    entry->scope = scope;
    entry->line = line;
    entry->isActive = 1;
    entry->type = type;

    entry->nextInScope = table->scopeLists[scope];
    table->scopeLists[scope] = entry;

    pNewBinding = malloc(sizeof(Binding));
    assert(pNewBinding);

    pNewBinding->key = strdup(name);
    assert(pNewBinding->key);

    pNewBinding->entry = entry;
    unsigned int index = SymTable_hash(name);
    pNewBinding->next = table->buckets[index];
    table->buckets[index] = pNewBinding;

    table->length++;
    return entry;
}

SymbolTableEntry *SymTable_Lookup(SymTable *table, const char *name, unsigned int scope) {
    
    SymbolTableEntry *pCurrent;
    
    assert(table);
    assert(name);

    pCurrent = table->scopeLists[scope];
    while(pCurrent){
        if(pCurrent->isActive && strcmp(pCurrent->name, name) == 0) return pCurrent;
        pCurrent = pCurrent->nextInScope;
    }
    return NULL;
}

SymbolTableEntry *SymTable_LookupAny(SymTable *table, const char *name) {
    
    SymbolTableEntry *pCurrent;
    
    assert(table);
    assert(name);

    for(int s = MAX_SCOPE - 1; s >= 0; s--){
        pCurrent = table->scopeLists[s];
        while(pCurrent){
            if (pCurrent->isActive && strcmp(pCurrent->name, name) == 0) return pCurrent;
            pCurrent = pCurrent->nextInScope;
        }
    }
    return NULL;
}

void SymTable_Hide(SymTable *table, unsigned int scope) {

    SymbolTableEntry *pCurrent;

    assert(table);

    pCurrent = table->scopeLists[scope];
    while (pCurrent) {
        pCurrent->isActive = 0;
        pCurrent = pCurrent->nextInScope;
    }
}

SymTable* SymTable_Initialize(void) {
    SymTable* table = SymTable_New();

    const char* libfuncs[] = {
        "print", "input", "objectmemberkeys", "objecttotalmembers", "objectcopy",
        "totalarguments", "argument", "typeof", "strtonum", "sqrt", "cos", "sin"
    };

    int num_funcs = sizeof(libfuncs) / sizeof(libfuncs[0]);

    for (int i = 0; i < num_funcs; i++) {
        SymTable_Insert(table, libfuncs[i], 0, 0, LIBFUNC);
        printf("Inserted library function '%s' into global scope\n", libfuncs[i]);
    }

    return table;
}

void SymTable_Print(SymTable *table) {
    printf("%-20s %-10s %-6s %-5s\n", "Name", "Type", "Line", "Scope");
    printf("-----------------------------------------------------\n");
    for (int s = 0; s < MAX_SCOPE; s++) {
        SymbolTableEntry *curr = table->scopeLists[s];
        while (curr) {
            if (curr->isActive) {
                const char *typeStr =
                    curr->type == GLOBAL_VAR ? "global" :
                    curr->type == LOCAL_VAR ? "local" :
                    curr->type == FORMAL ? "formal" :
                    curr->type == USERFUNC ? "userfunc" : "libfunc";
                printf("%-20s %-10s %-6u %-5u\n", curr->name, typeStr, curr->line, curr->scope);
            }
            curr = curr->nextInScope;
        }
    }
}