#include "../headers/avm_libFunc.h"
#include "../headers/avm_memcell.h"
#include "../headers/avm_tables.h"
#include "../headers/avm.h"
#include "../../alpha_parser_src/headers/stack.h"
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Helper functions for printing (mainly tables) */
int is_table_visited(avm_table *t, Stack *s) {
    Stack *temp_stack = newStack();
    int found = 0;

    while(!isEmptyStack(s)) {
        void *item = popStack(s);

        if(item == t)
            found = 1;

        pushStack(temp_stack, item);
    }

    while(!isEmptyStack(temp_stack))
        pushStack(s, popStack(temp_stack));

    destroyStack(temp_stack);
    return found;
}
void print_recursive(avm_memcell *m, Stack *visited_tables) {
    if(!m)
        return;

    if(m->type != table_m) {
        char *s = avm_tostring(m);
        printf("%s", s);
        free(s);
        return;
    }

    if(is_table_visited(m->data.tableVal, visited_tables)) {
        printf("[...]");
        return;
    }

    pushStack(visited_tables, m->data.tableVal);
    printf("[");
    avm_table *t = m->data.tableVal;
    int first = 1;

    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; ++i) {
        for(avm_table_bucket *b = t->numIndexed[i]; b; b = b->next) {
            if(!first)
                printf(", ");

            first = 0;
            print_recursive(&b->key, visited_tables);
        }
    }

    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; ++i) {
        for(avm_table_bucket *b = t->strIndexed[i]; b; b = b->next) {
            if(!first)
                printf(", ");

            first = 0;
            print_recursive(&b->key, visited_tables);
        }
    }
    printf("]");
    popStack(visited_tables);
}

typedef struct library_entry {
    char *name;
    library_func_t func;
    struct library_entry *next;
} library_entry;

static library_entry *library_head = NULL;

void avm_registerlibfunc(char *id, library_func_t addr) {
    library_entry *entry = malloc(sizeof(library_entry));
    assert(entry);
    entry->name = strdup(id);
    entry->func = addr;
    entry->next = library_head;
    library_head = entry;
}

library_func_t avm_getlibraryfunc(char *id) {
    library_entry *entry = library_head;

    while(entry) {
        if(strcmp(entry->name, id) == 0)
            return entry->func;

        entry = entry->next;
    }

    return NULL;
}

void avm_calllibfunc(char *funcName) {
    library_func_t f = avm_getlibraryfunc(funcName);

    if(!f) {
        avm_error("unsupported lib func '%s' called!", funcName);
        vm.executionFinished = 1;
    } else {
        (*f)();
    }
}

void avm_initlibraryfuncs(void) {
    avm_registerlibfunc("print", libfunc_print);
    avm_registerlibfunc("input", libfunc_input);
    avm_registerlibfunc("objectmemberkeys", libfunc_objectmemberkeys);
    avm_registerlibfunc("objecttotalmembers", libfunc_objecttotalmembers);
    avm_registerlibfunc("objectcopy", libfunc_objectcopy);
    avm_registerlibfunc("totalarguments", libfunc_totalarguments);
    avm_registerlibfunc("argument", libfunc_argument);
    avm_registerlibfunc("typeof", libfunc_typeof);
    avm_registerlibfunc("strtonum", libfunc_strtonum);
    avm_registerlibfunc("sqrt", libfunc_sqrt);
    avm_registerlibfunc("cos", libfunc_cos);
    avm_registerlibfunc("sin", libfunc_sin);
}

unsigned avm_totalactuals(void) {
    return vm.totalActuals;
}

avm_memcell *avm_getactual(unsigned i) {
    assert(i < vm.totalActuals);
    return &vm.stack[vm.top + 1 + i];
}

void libfunc_print(void) {
    unsigned n = avm_totalactuals();
    Stack *visited_tables = newStack();

    for(unsigned i = 0; i < n; ++i) {
        print_recursive(avm_getactual(i), visited_tables);

        if(i < n - 1)
            printf(" ");
    }
    printf("\n");
    destroyStack(visited_tables);
}

void libfunc_input(void) {
    char buffer[256];
    avm_memcellclear(&vm.retval);

    if(fgets(buffer, sizeof(buffer), stdin)) {
        size_t len = strlen(buffer);

        if(len > 0 && buffer[len - 1] == '\n')
            buffer[len - 1] = '\0';

        if(strcmp(buffer, "true") == 0) {
            vm.retval.type = bool_m;
            vm.retval.data.boolVal = 1;
        } else if(strcmp(buffer, "false") == 0) {
            vm.retval.type = bool_m;
            vm.retval.data.boolVal = 0;
        } else if(strcmp(buffer, "nil") == 0) {
            vm.retval.type = nil_m;
        } else {
            char *endptr;
            double num = strtod(buffer, &endptr);

            if(*endptr == '\0' && strlen(buffer) > 0) {
                vm.retval.type = number_m;
                vm.retval.data.numVal = num;
            } else {
                vm.retval.type = string_m;
                vm.retval.data.strVal = strdup(buffer);
            }
        }
    } else {
        vm.retval.type = nil_m;
    }
}

void libfunc_objectmemberkeys(void) {
    unsigned n = avm_totalactuals();

    if(n != 1) {
        avm_error("one argument (not %d) expected in 'objectmemberkeys'!", n);
        return;
    }

    avm_memcell *arg = avm_getactual(0);

    if(arg->type != table_m) {
        avm_error("argument to 'objectmemberkeys' must be a table!");
        return;
    }

    avm_memcellclear(&vm.retval);
    vm.retval.type = table_m;
    vm.retval.data.tableVal = avm_tablenew();
    avm_tableincrefcounter(vm.retval.data.tableVal);
    avm_table *source_table = arg->data.tableVal;
    unsigned key_index = 0;

    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        for(avm_table_bucket *bucket = source_table->strIndexed[i]; bucket; bucket = bucket->next) {
            avm_memcell index_cell;
            index_cell.type = number_m;
            index_cell.data.numVal = (double)key_index++;
            avm_tablesetelem(vm.retval.data.tableVal, &index_cell, &bucket->key);
        }
    }

    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        for(avm_table_bucket *bucket = source_table->numIndexed[i]; bucket; bucket = bucket->next) {
            avm_memcell index_cell;
            index_cell.type = number_m;
            index_cell.data.numVal = (double)key_index++;
            avm_tablesetelem(vm.retval.data.tableVal, &index_cell, &bucket->key);
        }
    }
}

void libfunc_objecttotalmembers(void) {
    unsigned n = avm_totalactuals();

    if(n != 1) {
        avm_error("one argument (not %d) expected in 'objecttotalmembers'!", n);
        return;
    }

    avm_memcell *arg = avm_getactual(0);

    if(arg->type != table_m) {
        avm_error("argument to 'objecttotalmembers' must be a table!");
        return;
    }

    avm_memcellclear(&vm.retval);
    vm.retval.type = number_m;
    vm.retval.data.numVal = arg->data.tableVal->total;
}

void libfunc_objectcopy(void) {
    unsigned n = avm_totalactuals();

    if(n != 1) {
        avm_error("one argument (not %d) expected in 'objectcopy'!", n);
        return;
    }

    avm_memcell *arg = avm_getactual(0);

    if(arg->type != table_m) {
        avm_error("argument to 'objectcopy' must be a table!");
        return;
    }

    /* new table for the copy */
    avm_memcellclear(&vm.retval);
    vm.retval.type = table_m;
    vm.retval.data.tableVal = avm_tablenew();
    avm_tableincrefcounter(vm.retval.data.tableVal);

    /* copy all elements from table */
    avm_table *source_table = arg->data.tableVal;

    /* string-indexed buckets */
    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        avm_table_bucket *bucket = source_table->strIndexed[i];

        while(bucket) {
            /* if value is a table, copy it */
            avm_memcell copied_value;
            memset(&copied_value, 0, sizeof(avm_memcell));

            if(bucket->value.type == table_m) {
                /* Recursive table copy */
                copied_value.type = table_m;
                copied_value.data.tableVal = avm_tablenew();
                avm_tableincrefcounter(copied_value.data.tableVal);

                /* copy all elements from nested table */
                avm_table *nested_source = bucket->value.data.tableVal;

                /* copy nested string-indexed buckets */
                for(unsigned j = 0; j < AVM_TABLE_HASHSIZE; j++) {
                    avm_table_bucket *nested_bucket = nested_source->strIndexed[j];

                    while(nested_bucket) {
                        avm_tablesetelem(copied_value.data.tableVal,
                                         &nested_bucket->key,
                                         &nested_bucket->value);
                        nested_bucket = nested_bucket->next;
                    }
                }

                /* copy nested number-indexed buckets */
                for(unsigned j = 0; j < AVM_TABLE_HASHSIZE; j++) {
                    avm_table_bucket *nested_bucket = nested_source->numIndexed[j];

                    while(nested_bucket) {
                        avm_tablesetelem(copied_value.data.tableVal,
                                         &nested_bucket->key,
                                         &nested_bucket->value);
                        nested_bucket = nested_bucket->next;
                    }
                }
            } else avm_assign(&copied_value, &bucket->value);

            /* add copied key-value to result table */
            avm_tablesetelem(vm.retval.data.tableVal, &bucket->key, &copied_value);

            /* clean up */
            if(copied_value.type == table_m) avm_memcellclear(&copied_value);

            bucket = bucket->next;
        }
    }

    /* copy number-indexed buckets */
    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        avm_table_bucket *bucket = source_table->numIndexed[i];

        while(bucket) {
            /* if value is a table, copy it */
            avm_memcell copied_value;
            memset(&copied_value, 0, sizeof(avm_memcell));

            if(bucket->value.type == table_m) {
                /* Recursive table copy */
                copied_value.type = table_m;
                copied_value.data.tableVal = avm_tablenew();
                avm_tableincrefcounter(copied_value.data.tableVal);

                /* copy all elements from nested table */
                avm_table *nested_source = bucket->value.data.tableVal;

                /* copy nested string-indexed buckets */
                for(unsigned j = 0; j < AVM_TABLE_HASHSIZE; j++) {
                    avm_table_bucket *nested_bucket = nested_source->strIndexed[j];

                    while(nested_bucket) {
                        avm_tablesetelem(copied_value.data.tableVal,
                                         &nested_bucket->key,
                                         &nested_bucket->value);
                        nested_bucket = nested_bucket->next;
                    }
                }

                /* copy nested number-indexed buckets */
                for(unsigned j = 0; j < AVM_TABLE_HASHSIZE; j++) {
                    avm_table_bucket *nested_bucket = nested_source->numIndexed[j];

                    while(nested_bucket) {
                        avm_tablesetelem(copied_value.data.tableVal,
                                         &nested_bucket->key,
                                         &nested_bucket->value);
                        nested_bucket = nested_bucket->next;
                    }
                }
            } else avm_assign(&copied_value, &bucket->value);

            /* add copied key-value to result table */
            avm_tablesetelem(vm.retval.data.tableVal, &bucket->key, &copied_value);

            /* clean up */
            if(copied_value.type == table_m) {
                avm_memcellclear(&copied_value);
            }

            bucket = bucket->next;
        }
    }
}

void libfunc_totalarguments(void) {
    //debug_print_stack_state("libfunc_totalarguments ENTRY");

    if(vm.topsp >= AVM_STACKSIZE - vm.programVarCount) {
        avm_error("'totalarguments' called outside a function!");
        vm.retval.type = nil_m;
        return;
    }

    unsigned env_addr = vm.topsp + AVM_NUMACTUALS_OFFSET;
    if(env_addr > AVM_STACKSIZE || env_addr < 1) {
        avm_error("Invalid stack access in 'totalarguments'!");
        vm.retval.type = nil_m;
        return;
    }

    avm_memcellclear(&vm.retval);
    vm.retval.type = number_m;
    vm.retval.data.numVal = avm_get_envvalue(env_addr);

    //debug_print_stack_state("libfunc_totalarguments EXIT");
}

void libfunc_argument(void) {
    //debug_print_stack_state("libfunc_argument ENTRY");

    if(vm.topsp >= AVM_STACKSIZE - vm.programVarCount) {
        avm_error("'argument' called outside a function!");
        vm.retval.type = nil_m;
        return;
    }

    unsigned n = avm_totalactuals();

    if(n != 1) {
        avm_error("one argument (not %d) expected in 'argument'!", n);
        return;
    }

    avm_memcell *arg = avm_getactual(0);

    if(arg->type != number_m) {
        avm_error("argument index must be a number!");
        return;
    }

    unsigned index = (unsigned)arg->data.numVal;
    unsigned total = avm_get_envvalue(vm.topsp + AVM_NUMACTUALS_OFFSET);

    //printf("DEBUG: argument(%u) - total args available: %u\n", index, total);

    if(index >= total) {
        avm_error("argument index %u out of range (total: %u)!", index, total);
        vm.retval.type = nil_m;
        return;
    }

    avm_memcellclear(&vm.retval);
    avm_assign(&vm.retval, &vm.stack[vm.topsp + AVM_STACKENV_SIZE + 1 + index]);

    //debug_print_stack_state("libfunc_argument EXIT");
}

void libfunc_typeof(void) {
    unsigned n = avm_totalactuals();

    if(n != 1) {
        avm_error("one argument (not %d) expected in 'typeof'!", n);
        return;
    }

    avm_memcell *arg = avm_getactual(0);
    avm_memcellclear(&vm.retval);
    vm.retval.type = string_m;
    vm.retval.data.strVal = strdup(typeStrings[arg->type]);
}

void libfunc_strtonum(void) {
    unsigned n = avm_totalactuals();

    if(n != 1) {
        avm_error("one argument (not %d) expected in 'strtonum'!", n);
        return;
    }

    avm_memcell *arg = avm_getactual(0);

    if(arg->type != string_m) {
        avm_error("argument to 'strtonum' must be a string!");
        return;
    }

    avm_memcellclear(&vm.retval);

    if(arg->data.strVal == NULL || arg->data.strVal[0] == '\0') {
        vm.retval.type = nil_m;
        return;
    }

    char *endptr;
    double result = strtod(arg->data.strVal, &endptr);

    if(endptr == arg->data.strVal) {
        vm.retval.type = nil_m;
        return;
    }

    while(isspace((unsigned char) * endptr)) {
        endptr++;
    }

    if(*endptr == '\0') {
        vm.retval.type = number_m;
        vm.retval.data.numVal = result;
    } else {
        vm.retval.type = nil_m;
    }
}

void libfunc_sqrt(void) {
    unsigned n = avm_totalactuals();

    if(n != 1) {
        avm_error("one argument (not %d) expected in 'sqrt'!", n);
        return;
    }

    avm_memcell *arg = avm_getactual(0);

    if(arg->type != number_m) {
        avm_error("argument to 'sqrt' must be a number!");
        return;
    }

    avm_memcellclear(&vm.retval);

    if(arg->data.numVal < 0) {
        vm.retval.type = nil_m;
    } else {
        vm.retval.type = number_m;
        vm.retval.data.numVal = sqrt(arg->data.numVal);
    }
}

void libfunc_cos(void) {
    unsigned n = avm_totalactuals();

    if(n != 1) {
        avm_error("one argument (not %d) expected in 'cos'!", n);
        return;
    }

    avm_memcell *arg = avm_getactual(0);

    if(arg->type != number_m) {
        avm_error("argument to 'cos' must be a number!");
        return;
    }

    avm_memcellclear(&vm.retval);
    double degrees = arg->data.numVal;
    double radians = degrees * M_PI / 180.0;
    vm.retval.type = number_m;
    vm.retval.data.numVal = cos(radians);
}

void libfunc_sin(void) {
    unsigned n = avm_totalactuals();

    if(n != 1) {
        avm_error("one argument (not %d) expected in 'sin'!", n);
        return;
    }

    avm_memcell *arg = avm_getactual(0);

    if(arg->type != number_m) {
        avm_error("argument to 'sin' must be a number!");
        return;
    }

    avm_memcellclear(&vm.retval);
    double degrees = arg->data.numVal;
    double radians = degrees * M_PI / 180.0;
    vm.retval.type = number_m;
    vm.retval.data.numVal = sin(radians);
}