#include "../headers/avm_libFunc.h"
#include "../headers/avm_memcell.h"
#include "../headers/avm_tables.h"
#include "../headers/avm.h"
#include <math.h>

/* Library function registry */
typedef struct library_entry {
    char *name;
    library_func_t func;
    struct library_entry *next;
} library_entry;

static library_entry *library_head = NULL;
static unsigned lib_arg_start = 0;
static unsigned lib_arg_count = 0;

/* Register a library function */
void avm_registerlibfunc(char *id, library_func_t addr) {
    library_entry *entry = malloc(sizeof(library_entry));
    assert(entry);
    
    entry->name = strdup(id);
    entry->func = addr;
    entry->next = library_head;
    library_head = entry;
}

/* Get library function by name */
library_func_t avm_getlibraryfunc(char *id) {
    library_entry *entry = library_head;
    
    while(entry) {
        if(strcmp(entry->name, id) == 0) return entry->func;
        entry = entry->next;
    }
    
    return NULL;
}

/* Call library function by name */
void avm_calllibfunc(char *funcName) {
    printf("DEBUG: avm_calllibfunc - calling: %s\n", funcName);
    printf("DEBUG: avm_calllibfunc - current top: %u, topsp: %u\n", vm.top, vm.topsp);
    
    library_func_t f = avm_getlibraryfunc(funcName);
    
    if(!f) {
        avm_error("unsupported lib func '%s' called!", funcName);
        vm.executionFinished = 1;
    } else {
        // FIX: For library functions, arguments are between top+1 and topsp
        if(vm.topsp > vm.top) {
            lib_arg_start = vm.top + 1;
            lib_arg_count = vm.topsp - vm.top;
        } else {
            lib_arg_start = vm.topsp + 1;  
            lib_arg_count = vm.top - vm.topsp;
        }
        
        printf("DEBUG: avm_calllibfunc - lib_arg_start: %u, lib_arg_count: %u\n", lib_arg_start, lib_arg_count);
        
        // Validate the argument count by checking actual stack contents
        unsigned actual_count = 0;
        for(unsigned i = 0; i < lib_arg_count && i < 100; i++) { // Safety limit
            unsigned addr = lib_arg_start + i;
            
            if(addr >= AVM_STACKSIZE) {
                printf("DEBUG: avm_calllibfunc - addr %u out of bounds, stopping at %u args\n", addr, i);
                break;
            }
            
            avm_memcell *cell = &vm.stack[addr];
            printf("DEBUG: avm_calllibfunc - arg[%u] at addr %u, type: %d\n", i, addr, cell->type);
            
            if(cell && cell->type != undef_m) actual_count++;
            else break;
        }
        lib_arg_count = actual_count;
        printf("DEBUG: avm_calllibfunc - final lib_arg_count: %u\n", lib_arg_count);

        unsigned old_topsp = vm.topsp;
        vm.topsp = vm.top;
        printf("DEBUG: avm_calllibfunc - set topsp to top: %u (was %u)\n", vm.topsp, old_topsp);
        (*f)();
        printf("DEBUG: avm_calllibfunc - function %s completed\n", funcName);
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
    printf("DEBUG: avm_totalactuals - lib_arg_count: %u\n", lib_arg_count);
    return lib_arg_count; 
}

avm_memcell *avm_getactual(unsigned i) {    
    if(i >= lib_arg_count) {
        avm_error("avm_getactual: argument index %u out of range (total: %u)!", i, lib_arg_count);
        return NULL;
    }
    
    unsigned addr = lib_arg_start + i;
    
    if(addr >= AVM_STACKSIZE) {
        avm_error("avm_getactual: address %u out of bounds", addr);
        return NULL;
    }
    
    avm_memcell *cell = &vm.stack[addr];
    
    return cell;
}


void libfunc_print(void) {
    unsigned n = avm_totalactuals();
    unsigned i;
    
    for(i = 0; i < n; ++i) {
        avm_memcell *arg = avm_getactual(i);
        char *s = avm_tostring(arg);
        printf("%s", s);
        free(s);
    }
    printf("\n");
    fflush(stdout);
}

void libfunc_input(void) {
    char buffer[256];
    
    if(fgets(buffer, sizeof(buffer), stdin)) {
        size_t len = strlen(buffer);
        if(len > 0 && buffer[len-1] == '\n')
            buffer[len-1] = '\0';
        
        avm_memcellclear(&vm.retval);
        vm.retval.type = string_m;
        vm.retval.data.strVal = strdup(buffer);
    } else {
        avm_memcellclear(&vm.retval);
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
    
    /* new table for keys */
    avm_memcellclear(&vm.retval);
    vm.retval.type = table_m;
    vm.retval.data.tableVal = avm_tablenew();
    avm_tableincrefcounter(vm.retval.data.tableVal);
    
    avm_table *source_table = arg->data.tableVal;
    unsigned key_index = 0;
    
    /* string-indexed buckets */
    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        avm_table_bucket *bucket = source_table->strIndexed[i];
        while(bucket) {
            /* index for key */
            avm_memcell index_cell;
            index_cell.type = number_m;
            index_cell.data.numVal = (double)key_index++;
            
            /* add key to result table */
            avm_tablesetelem(vm.retval.data.tableVal, &index_cell, &bucket->key);
            
            bucket = bucket->next;
        }
    }
    
    /* number-indexed buckets */
    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        avm_table_bucket *bucket = source_table->numIndexed[i];
        while(bucket) {
            /* index for key */
            avm_memcell index_cell;
            index_cell.type = number_m;
            index_cell.data.numVal = (double)key_index++;
            
            /* add key to result table */
            avm_tablesetelem(vm.retval.data.tableVal, &index_cell, &bucket->key);
            
            bucket = bucket->next;
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
            /* if value is a table, recursively copy it */
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
            /* if value is a table, recursively copy it */
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
    printf("DEBUG: libfunc_totalarguments - entry: top=%u, topsp=%u\n", vm.top, vm.topsp);
    
    // We need to look in the current function's environment for the argument count
    // The environment is structured as: [numactuals][saved_pc][saved_top][saved_topsp]
    // We're currently in a user function, so we need to look at our environment
    
    // Check if we have a valid environment
    if(vm.topsp + AVM_NUMACTUALS_OFFSET >= AVM_STACKSIZE) {
        printf("DEBUG: libfunc_totalarguments - environment address out of bounds\n");
        avm_memcellclear(&vm.retval);
        vm.retval.type = number_m;
        vm.retval.data.numVal = 0;
        return;
    }
    
    // The number of actuals should be stored in the environment
    avm_memcell *numactuals_cell = &vm.stack[vm.topsp + AVM_NUMACTUALS_OFFSET];
    printf("DEBUG: libfunc_totalarguments - numactuals_cell at addr %u, type: %d\n", 
           vm.topsp + AVM_NUMACTUALS_OFFSET, numactuals_cell->type);
    
    if(numactuals_cell->type == number_m) {
        double arg_count = numactuals_cell->data.numVal;
        printf("DEBUG: libfunc_totalarguments - found arg_count: %f\n", arg_count);
        
        avm_memcellclear(&vm.retval);
        vm.retval.type = number_m;
        vm.retval.data.numVal = arg_count;
        return;
    }
    
    printf("DEBUG: libfunc_totalarguments - numactuals_cell not a number, returning 0\n");
    avm_memcellclear(&vm.retval);
    vm.retval.type = number_m;
    vm.retval.data.numVal = 0;
}

void libfunc_argument(void) {
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
    unsigned p_topsp = vm.stack[vm.topsp + AVM_SAVEDTOPSP_OFFSET].data.numVal;
    
    if(!p_topsp) {
        avm_error("'argument' called outside a function!");
        vm.retval.type = nil_m;
        return;
    }
    
    unsigned total = vm.stack[p_topsp + AVM_NUMACTUALS_OFFSET].data.numVal;
    if(index >= total) {
        avm_error("argument index %d out of range (total: %d)!", index, total);
        vm.retval.type = nil_m;
        return;
    }
    
    avm_memcellclear(&vm.retval);
    avm_assign(&vm.retval, &vm.stack[p_topsp + AVM_STACKENV_SIZE + 1 + index]);
}

void libfunc_typeof(void) {
    unsigned n = avm_totalactuals();
    
    if(n != 1) {
        avm_error("one argument (not %d) expected in 'typeof'!", n);
        return;
    }
    
    avm_memcellclear(&vm.retval);
    vm.retval.type = string_m;
    vm.retval.data.strVal = strdup(typeStrings[avm_getactual(0)->type]);
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
    
    double result = strtod(arg->data.strVal, NULL);
    
    avm_memcellclear(&vm.retval);
    vm.retval.type = number_m;
    vm.retval.data.numVal = result;
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
    
    if(arg->data.numVal < 0) {
        avm_error("sqrt of negative number!");
        return;
    }
    
    avm_memcellclear(&vm.retval);
    vm.retval.type = number_m;
    vm.retval.data.numVal = sqrt(arg->data.numVal);
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
    vm.retval.type = number_m;
    vm.retval.data.numVal = cos(arg->data.numVal);
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
    vm.retval.type = number_m;
    vm.retval.data.numVal = sin(arg->data.numVal);
}