#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "symbol_table.h"

#define hash_multi 65599

static unsigned int hash(const char *key) {
    unsigned int hash = 0U;
    size_t i;
    for(i = 0U; key[i] != '\0'; i++) hash = hash * hash_multi + key[i];
    return hash % HASH_SIZE;
}

SymTable *SymTable_new(void) {

    SymTable *table = malloc(sizeof(SymTable));
    assert(table);
    table->length = 0;

    for(int i = 0; i < HASH_SIZE; i++) table->buckets[i] = NULL;
    for(int i = 0; i < MAX_SCOPE; i++) table->scopeLists[i] = NULL;
    return table;
}

void SymTable_free(SymTable *table) {
    assert(table);

    for(int i = 0; i < HASH_SIZE; i++){
        Binding *b = table->buckets[i];
        while (b) {
            Binding *next = b->next;
            free(b->key);
            free(b->entry->name);
            free(b->entry);
            free(b);
            b = next;
        }
    }
    free(table);
}

unsigned int SymTable_getLength(SymTable *table) {
    assert(table);
    return table->length;
}

SymbolTableEntry *SymTable_insert(SymTable *table, const char *name, unsigned int scope, unsigned int line, SymbolType type) {
    assert(table && name);

    SymbolTableEntry *existing = SymTable_lookup(table, name, scope);
    if (existing && existing->isActive) return NULL;

    SymbolTableEntry *entry = malloc(sizeof(SymbolTableEntry));
    assert(entry);

    entry->name = strdup(name);
    entry->scope = scope;
    entry->line = line;
    entry->isActive = true;
    entry->type = type;

    entry->nextInScope = table->scopeLists[scope];
    table->scopeLists[scope] = entry;

    Binding *binding = malloc(sizeof(Binding));
    assert(binding);

    binding->key = strdup(name);
    binding->entry = entry;
    unsigned int index = hash(name);
    binding->next = table->buckets[index];
    table->buckets[index] = binding;

    table->length++;
    return entry;
}

SymbolTableEntry *SymTable_lookup(SymTable *table, const char *name, unsigned int scope) {
    assert(table && name);

    SymbolTableEntry *curr = table->scopeLists[scope];
    while (curr) {
        if (curr->isActive && strcmp(curr->name, name) == 0) return curr;
        curr = curr->nextInScope;
    }
    return NULL;
}

SymbolTableEntry *SymTable_lookup_any(SymTable *table, const char *name) {
    assert(table && name);

    for (int s = MAX_SCOPE - 1; s >= 0; s--) {
        SymbolTableEntry *curr = table->scopeLists[s];
        while (curr) {
            if (curr->isActive && strcmp(curr->name, name) == 0) return curr;
            curr = curr->nextInScope;
        }
    }
    return NULL;
}

void SymTable_hide(SymTable *table, unsigned int scope) {
    assert(table);

    SymbolTableEntry *curr = table->scopeLists[scope];
    while (curr) {
        curr->isActive = false;
        curr = curr->nextInScope;
    }
}

void SymTable_print(SymTable *table) {
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