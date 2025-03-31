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

typedef struct SymbolTableEntry {

    char *name;
    unsigned int scope;
    unsigned int line;
    int isActive;
    SymbolType type;

    struct SymbolTableEntry *nextInScope;
    struct SymbolTableEntry *nextLink;

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


SymTable *SymTable_New(void);

void SymTable_Free(SymTable *table);

unsigned int SymTable_getLength(SymTable *table);

SymbolTableEntry *SymTable_Insert(SymTable *table, const char *name, unsigned int scope, unsigned int line, SymbolType type);

SymbolTableEntry *SymTable_Lookup(SymTable *table, const char *name, unsigned int scope);

SymbolTableEntry *SymTable_LookupAny(SymTable *table, const char *name);

void SymTable_Hide(SymTable *table, unsigned int scope);

SymTable* SymTable_Initialize(void);

void SymTable_Print(SymTable *table);

#endif
