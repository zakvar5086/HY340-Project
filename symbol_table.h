#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <stdbool.h>

#define HASH_SIZE 509
#define MAX_SCOPE 256

typedef enum SymbolType {
    GLOBAL_VAR,
    LOCAL_VAR,
    FORMAL,
    USERFUNC,
    LIBFUNC
} SymbolType;

typedef struct SymbolTableEntry {
    char *name;
    unsigned int scope;
    unsigned int line;
    bool isActive;
    SymbolType type;

    struct SymbolTableEntry *nextInScope;
} SymbolTableEntry;

typedef struct Binding {
    char *key;
    SymbolTableEntry *entry;
    struct Binding *next;
} Binding;

typedef struct SymTable {
    unsigned int length;
    Binding *buckets[HASH_SIZE];
    SymbolTableEntry *scopeLists[MAX_SCOPE];
} SymTable;


SymTable *SymTable_new(void);

void SymTable_free(SymTable *table);

unsigned int SymTable_getLength(SymTable *table);

SymbolTableEntry *SymTable_insert(SymTable *table, const char *name, unsigned int scope, unsigned int line, SymbolType type);

SymbolTableEntry *SymTable_lookup(SymTable *table, const char *name, unsigned int scope);

SymbolTableEntry *SymTable_lookup_any(SymTable *table, const char *name);

void SymTable_hide(SymTable *table, unsigned int scope);

void SymTable_print(SymTable *table);

#endif
