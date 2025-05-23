#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#define HASH_SIZE 509
#define MAX_SCOPE 100

typedef enum SymbolType {
    GLOBAL_VAR,
    LOCAL_VAR,
    FORMAL,
    USERFUNC,
    LIBFUNC
} SymbolType;

typedef struct SymTableEntry {

    char *name;
    unsigned int scope;
    unsigned int line;
    unsigned int offset;
    int isActive;
    SymbolType type;

    struct SymTableEntry *nextInScope;
    struct SymTableEntry *nextLink;

} SymTableEntry;

typedef struct Binding {

    char *key;
    SymTableEntry *entry;
    struct Binding *next;

} Binding;

typedef struct SymTable {

    unsigned int length;
    Binding *buckets[HASH_SIZE];
    SymTableEntry *scopeLists[MAX_SCOPE];

} SymTable;


SymTable *SymTable_New(void);

void SymTable_Free(SymTable *table);

unsigned int SymTable_getLength(SymTable *table);

SymTableEntry *SymTable_Insert(SymTable *table, const char *name, unsigned int scope, unsigned int line, SymbolType type);

SymTableEntry *SymTable_Lookup(SymTable *table, const char *name, unsigned int scope);

SymTableEntry *SymTable_LookupAny(SymTable *table, const char *name);

void SymTable_Hide(SymTable *table, unsigned int scope);

SymTable* SymTable_Initialize(void);

void SymTable_Print(SymTable *table);

#endif
