#ifndef AVM_TABLES_H
#define AVM_TABLES_H

#include "avm_memcell.h"

#define AVM_TABLE_HASHSIZE 211

/* Forward declaration */
typedef struct avm_table_bucket avm_table_bucket;

/* Hash table bucket for dynamic associative arrays */
struct avm_table_bucket {
    avm_memcell key;
    avm_memcell value;
    avm_table_bucket *next;
};

/* Dynamic associative array (table) structure */
typedef struct avm_table {
    unsigned refCounter;
    avm_table_bucket *strIndexed[AVM_TABLE_HASHSIZE];
    avm_table_bucket *numIndexed[AVM_TABLE_HASHSIZE];
    unsigned total;
} avm_table;

/* Table lifecycle management */
avm_table *avm_tablenew(void);
void avm_tabledestroy(avm_table *t);
void avm_tableincrefcounter(avm_table *t);
void avm_tabledecrefcounter(avm_table *t);

/* Table operations */
avm_memcell *avm_tablegetelem(avm_table *table, avm_memcell *index);
void avm_tablesetelem(avm_table *table, avm_memcell *index, avm_memcell *content);

/* Hash functions */
unsigned avm_hash_string(char *s);
unsigned avm_hash_number(double n);

/* Bucket management */
avm_table_bucket *avm_tablebucket_create(avm_memcell *key, avm_memcell *value);
void avm_tablebucket_destroy(avm_table_bucket *bucket);

/* Internal helpers */
avm_table_bucket **avm_tablegetbucket(avm_table *table, avm_memcell *index);

#endif