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
    library_func_t f = avm_getlibraryfunc(funcName);
    
    if(!f) {
        avm_error("unsupported lib func '%s' called!", funcName);
        vm.executionFinished = 1;
    } else {
        unsigned saved_topsp = vm.topsp;
        vm.totalActuals = 0;
        (*f)();
        
        vm.topsp = saved_topsp;
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

/* Get total actuals from current activation record */
unsigned avm_totalactuals(void) {
    if(vm.topsp + AVM_NUMACTUALS_OFFSET > AVM_STACKSIZE) {
        avm_error("topsp + offset (%u) exceeds stack bounds", vm.topsp + AVM_NUMACTUALS_OFFSET);
        return 0;
    }
    return avm_get_envvalue(vm.topsp + AVM_NUMACTUALS_OFFSET);
}

/* Get the i-th actual argument from current activation record */
avm_memcell *avm_getactual(unsigned i) {
    unsigned total = avm_totalactuals();
    if(i >= total) {
        avm_error("argument index %u out of range (total: %u)", i, total);
        return &vm.stack[0]; // Return dummy cell to avoid crash
    }
    
    unsigned addr = vm.topsp + AVM_STACKENV_SIZE + 1 + i;
    if(addr > AVM_STACKSIZE) {
        avm_error("actual argument address %u out of bounds", addr);
        return &vm.stack[0]; // Return dummy cell to avoid crash
    }
    
    return &vm.stack[addr];
}

void libfunc_print(void) {
    unsigned n = avm_totalactuals();
    for(unsigned i = 0; i < n; ++i) {
        char *s = avm_tostring(avm_getactual(i));
        printf("%s", s);
        free(s);
    }
    printf("\n");
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
    
    /* Create new table for keys */
    avm_memcellclear(&vm.retval);
    vm.retval.type = table_m;
    vm.retval.data.tableVal = avm_tablenew();
    avm_tableincrefcounter(vm.retval.data.tableVal);
    
    avm_table *source_table = arg->data.tableVal;
    unsigned key_index = 0;
    
    /* Copy string-indexed keys */
    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        avm_table_bucket *bucket = source_table->strIndexed[i];
        while(bucket) {
            avm_memcell index_cell;
            index_cell.type = number_m;
            index_cell.data.numVal = (double)key_index++;
            
            avm_tablesetelem(vm.retval.data.tableVal, &index_cell, &bucket->key);
            bucket = bucket->next;
        }
    }
    
    /* Copy number-indexed keys */
    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        avm_table_bucket *bucket = source_table->numIndexed[i];
        while(bucket) {
            avm_memcell index_cell;
            index_cell.type = number_m;
            index_cell.data.numVal = (double)key_index++;
            
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
    
    /* Create new table for the copy */
    avm_memcellclear(&vm.retval);
    vm.retval.type = table_m;
    vm.retval.data.tableVal = avm_tablenew();
    avm_tableincrefcounter(vm.retval.data.tableVal);
    
    avm_table *source_table = arg->data.tableVal;
    
    /* Copy string-indexed buckets */
    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        avm_table_bucket *bucket = source_table->strIndexed[i];
        while(bucket) {
            avm_memcell copied_value;
            memset(&copied_value, 0, sizeof(avm_memcell));
            
            if(bucket->value.type == table_m) {
                /* TODO: Recursive table copy */
                avm_assign(&copied_value, &bucket->value);
            } else {
                avm_assign(&copied_value, &bucket->value);
            }
            
            avm_tablesetelem(vm.retval.data.tableVal, &bucket->key, &copied_value);
            
            if(copied_value.type == table_m) avm_memcellclear(&copied_value);
            
            bucket = bucket->next;
        }
    }
    
    /* Copy number-indexed buckets */
    for(unsigned i = 0; i < AVM_TABLE_HASHSIZE; i++) {
        avm_table_bucket *bucket = source_table->numIndexed[i];
        while(bucket) {
            avm_memcell copied_value;
            memset(&copied_value, 0, sizeof(avm_memcell));
            
            if(bucket->value.type == table_m) {
                /* TODO: Recursive table copy */
                avm_assign(&copied_value, &bucket->value);
            } else {
                avm_assign(&copied_value, &bucket->value);
            }
            
            avm_tablesetelem(vm.retval.data.tableVal, &bucket->key, &copied_value);
            
            if(copied_value.type == table_m) avm_memcellclear(&copied_value);
            
            bucket = bucket->next;
        }
    }
}

void libfunc_totalarguments(void) {
    unsigned p_topsp = avm_get_envvalue(vm.topsp + AVM_SAVEDTOPSP_OFFSET);
    
    if(!p_topsp) {
        avm_error("'totalarguments' called outside a function!");
        vm.retval.type = nil_m;
    } else {
        avm_memcellclear(&vm.retval);
        vm.retval.type = number_m;
        vm.retval.data.numVal = avm_get_envvalue(p_topsp + AVM_NUMACTUALS_OFFSET);
    }
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
    unsigned p_topsp = avm_get_envvalue(vm.topsp + AVM_SAVEDTOPSP_OFFSET);
    
    if(!p_topsp) {
        avm_error("'argument' called outside a function!");
        vm.retval.type = nil_m;
        return;
    }
    
    unsigned total = avm_get_envvalue(p_topsp + AVM_NUMACTUALS_OFFSET);
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
    } else {
        avm_memcellclear(&vm.retval);
        vm.retval.type = string_m;
        vm.retval.data.strVal = strdup(typeStrings[avm_getactual(0)->type]);
    }
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
    
    char *endptr;
    double result = strtod(arg->data.strVal, &endptr);
    
    /* Check if conversion was successful */
    if(endptr == arg->data.strVal) {
        avm_warning("'%s' cannot be converted to number", arg->data.strVal);
        result = 0.0;
    }
    
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