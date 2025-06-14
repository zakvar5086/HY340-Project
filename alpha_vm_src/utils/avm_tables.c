#include "../headers/avm_tables.h"
#include "../headers/avm_memcell.h"
#include <assert.h>

extern void avm_memcellclear(avm_memcell *m);
extern void avm_assign(avm_memcell *lv, avm_memcell *rv);

/* Create a new table */
avm_table *avm_tablenew(void) {
    avm_table *t = (avm_table*)malloc(sizeof(avm_table));
    assert(t);

    t->refCounter = 0;
    t->total = 0;

    memset(t->strIndexed, 0, AVM_TABLE_HASHSIZE * sizeof(avm_table_bucket*));
    memset(t->numIndexed, 0, AVM_TABLE_HASHSIZE * sizeof(avm_table_bucket*));

    return t;
}

/* Destroy a table and all its contents */
void avm_tabledestroy(avm_table *t) {
    unsigned i;
    avm_table_bucket *bucket, *next;

    assert(t);

    /* Clear string-indexed buckets */
    for(i = 0; i < AVM_TABLE_HASHSIZE; ++i) {
        bucket = t->strIndexed[i];

        while(bucket) {
            next = bucket->next;
            avm_tablebucket_destroy(bucket);
            bucket = next;
        }
    }

    /* Clear number-indexed buckets */
    for(i = 0; i < AVM_TABLE_HASHSIZE; ++i) {
        bucket = t->numIndexed[i];

        while(bucket) {
            next = bucket->next;
            avm_tablebucket_destroy(bucket);
            bucket = next;
        }
    }

    free(t);
}

/* Increment table reference counter */
void avm_tableincrefcounter(avm_table *t) {
    assert(t);
    ++t->refCounter;
}

/* Decrement table reference counter and destroy if needed */
void avm_tabledecrefcounter(avm_table *t) {
    assert(t);
    assert(t->refCounter > 0);

    if(--t->refCounter == 0) avm_tabledestroy(t);
}

/* Get table element by index */
avm_memcell *avm_tablegetelem(avm_table *table, avm_memcell *index) {
    avm_table_bucket **bucket_ptr;
    avm_table_bucket *bucket;

    assert(table && index);

    bucket_ptr = avm_tablegetbucket(table, index);

    if(!bucket_ptr)
        return NULL;

    bucket = *bucket_ptr;

    while(bucket) {
        if(bucket->key.type == index->type) {
            switch(index->type) {
            case number_m:
                if(bucket->key.data.numVal == index->data.numVal)
                    return &bucket->value;

                break;

            case string_m:
                if(strcmp(bucket->key.data.strVal, index->data.strVal) == 0)
                    return &bucket->value;

                break;

            case bool_m:
                if(bucket->key.data.boolVal == index->data.boolVal)
                    return &bucket->value;

                break;

            case userfunc_m:
                if(bucket->key.data.funcVal == index->data.funcVal)
                    return &bucket->value;

                break;

            case libfunc_m:
                if(strcmp(bucket->key.data.libfuncVal, index->data.libfuncVal) == 0)
                    return &bucket->value;

                break;

            case nil_m:
                if(bucket->key.type == nil_m)
                    return &bucket->value;

                break;

            case table_m:
                if(bucket->key.data.tableVal == index->data.tableVal)
                    return &bucket->value;

                break;

            case undef_m:
                avm_error("undefined value used as table index!");
                return NULL;

            default:
                return NULL;
            }
        }

        bucket = bucket->next;
    }

    return NULL;
}

/* Set table element at index */
void avm_tablesetelem(avm_table *table, avm_memcell *index, avm_memcell *content) {
    avm_table_bucket **bucket_ptr;
    avm_table_bucket *bucket;

    assert(table && index);

    if(content && content->type == nil_m) {
        avm_memcell *existing = avm_tablegetelem(table, index);

        if(existing) {
            /* Find and remove the bucket */
            bucket_ptr = avm_tablegetbucket(table, index);

            if(bucket_ptr) {
                avm_table_bucket *prev = NULL;
                bucket = *bucket_ptr;

                while(bucket) {
                    int match = 0;

                    if(bucket->key.type == index->type) {
                        switch(index->type) {
                        case number_m:
                            match = (bucket->key.data.numVal == index->data.numVal);
                            break;

                        case string_m:
                            match = (strcmp(bucket->key.data.strVal, index->data.strVal) == 0);
                            break;

                        case bool_m:
                            match = (bucket->key.data.boolVal == index->data.boolVal);
                            break;

                        case userfunc_m:
                            match = (bucket->key.data.funcVal == index->data.funcVal);
                            break;

                        case libfunc_m:
                            match = (strcmp(bucket->key.data.libfuncVal, index->data.libfuncVal) == 0);
                            break;

                        case nil_m:
                            match = (bucket->key.type == nil_m);
                            break;

                        case table_m:
                            match = (bucket->key.data.tableVal == index->data.tableVal);
                            break;

                        case undef_m:
                            avm_error("undefined value used as table index!");
                            return;

                        default:
                            avm_error("illegal use of type %s as table!", typeStrings[index->type]);
                            return;
                        }
                    }

                    if(match) {
                        if(prev) prev->next = bucket->next;
                        else *bucket_ptr = bucket->next;

                        avm_tablebucket_destroy(bucket);
                        --table->total;
                        return;
                    }

                    prev = bucket;
                    bucket = bucket->next;
                }
            }
        }

        return;
    }

    bucket_ptr = avm_tablegetbucket(table, index);

    if(!bucket_ptr) {
        char *ts = avm_tostring(index);
        avm_error("illegal use of type %s as table!", typeStrings[index->type]);
        free(ts);
        return;
    }

    bucket = *bucket_ptr;

    while(bucket) {
        if(bucket->key.type == index->type) {
            int match = 0;

            switch(index->type) {
            case number_m:
                match = (bucket->key.data.numVal == index->data.numVal);
                break;

            case string_m:
                match = (strcmp(bucket->key.data.strVal, index->data.strVal) == 0);
                break;

            case bool_m:
                match = (bucket->key.data.boolVal == index->data.boolVal);
                break;

            case userfunc_m:
                match = (bucket->key.data.funcVal == index->data.funcVal);
                break;

            case libfunc_m:
                match = (strcmp(bucket->key.data.libfuncVal, index->data.libfuncVal) == 0);
                break;

            case nil_m:
                match = (bucket->key.type == nil_m);
                break;

            case table_m:
                match = (bucket->key.data.tableVal == index->data.tableVal);
                break;

            case undef_m:
                avm_error("undefined value used as table index!");
                return;

            default:
                avm_error("illegal use of type %s as table!", typeStrings[index->type]);
                return;
            }

            if(match) {
                if(content) avm_assign(&bucket->value, content);

                return;
            }
        }

        bucket = bucket->next;
    }

    if(content) {
        avm_table_bucket *new_bucket = avm_tablebucket_create(index, content);
        new_bucket->next = *bucket_ptr;
        *bucket_ptr = new_bucket;
        ++table->total;
    }
}

unsigned avm_hash_string(char *s) {
    unsigned hash = 0;

    while(*s) hash = hash * 65599 + *s++;

    return hash % AVM_TABLE_HASHSIZE;
}

/* Hash function for numbers */
unsigned avm_hash_number(double n) {
    return ((unsigned)n) % AVM_TABLE_HASHSIZE;
}

/* Create a new table bucket */
avm_table_bucket *avm_tablebucket_create(avm_memcell *key, avm_memcell *value) {
    avm_table_bucket *bucket = (avm_table_bucket*)malloc(sizeof(avm_table_bucket));
    assert(bucket);

    memset(&bucket->key, 0, sizeof(avm_memcell));
    memset(&bucket->value, 0, sizeof(avm_memcell));

    avm_assign(&bucket->key, key);
    avm_assign(&bucket->value, value);

    bucket->next = NULL;

    return bucket;
}

void avm_tablebucket_destroy(avm_table_bucket *bucket) {
    assert(bucket);

    avm_memcellclear(&bucket->key);
    avm_memcellclear(&bucket->value);

    free(bucket);
}

/* Get appropriate bucket array for index type */
avm_table_bucket **avm_tablegetbucket(avm_table *table, avm_memcell *index) {
    assert(table && index);

    switch(index->type) {
    case number_m: {
        unsigned hash = avm_hash_number(index->data.numVal);
        return &table->numIndexed[hash];
    }

    case string_m: {
        unsigned hash = avm_hash_string(index->data.strVal);
        return &table->strIndexed[hash];
    }

    case bool_m: {
        unsigned hash = index->data.boolVal ? 1 : 0; // Use 0 for false, 1 for true
        return &table->numIndexed[hash];
    }

    case userfunc_m: {
        unsigned hash = index->data.funcVal % AVM_TABLE_HASHSIZE;
        return &table->numIndexed[hash];
    }

    case libfunc_m: {
        unsigned hash = avm_hash_string(index->data.libfuncVal);
        return &table->strIndexed[hash];
    }

    case nil_m: {
        return &table->numIndexed[0]; // Use 0 for nil
    }

    case table_m: {
        unsigned hash = (unsigned)(uintptr_t)index->data.tableVal % AVM_TABLE_HASHSIZE;
        return &table->numIndexed[hash];
    }

    case undef_m:
        avm_error("undefined value used as table index!");
        return NULL;

    default:
        return NULL;
    }
}