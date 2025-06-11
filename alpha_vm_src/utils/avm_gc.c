#include "../headers/avm_gc.h"
#include "../headers/avm_tables.h"
#include <assert.h>

/* Dispatch table for memory cleanup functions */
memclear_func_t memclearFuncs[] = {
    memclear_number,
    memclear_string,
    memclear_bool,
    memclear_table,
    memclear_userfunc,
    memclear_libfunc,
    memclear_nil,
    memclear_undef
};

/* Main memory cell cleanup function using dispatch */
void avm_memcellclear(avm_memcell *m) {
    if (m->type != undef_m) {
        memclear_func_t f = memclearFuncs[m->type];
        if(f) (*f)(m);
        m->type = undef_m;
    }
}


void memclear_string(avm_memcell *m) {
    assert(m->data.strVal);
    free(m->data.strVal);
}

void memclear_table(avm_memcell *m) {
    assert(m->data.tableVal);
    avm_tabledecrefcounter(m->data.tableVal);
}

void memclear_number(avm_memcell *m)    { (void)*m; }
void memclear_bool(avm_memcell *m)      { (void)*m; }
void memclear_userfunc(avm_memcell *m)  { (void)*m; }
void memclear_libfunc(avm_memcell *m)   { (void)*m; }
void memclear_nil(avm_memcell *m)       { (void)*m; }
void memclear_undef(avm_memcell *m)     { (void)*m; }

void avm_initgc(void) {
    /* Verify dispatch table is properly sized */
    assert(sizeof(memclearFuncs)/sizeof(memclear_func_t) == 8);
    
    /* All function pointers should be valid */
    assert(memclearFuncs[number_m]  == memclear_number);
    assert(memclearFuncs[string_m]  == memclear_string);
    assert(memclearFuncs[bool_m]    == memclear_bool);
    assert(memclearFuncs[table_m]   == memclear_table);
    assert(memclearFuncs[userfunc_m]== memclear_userfunc);
    assert(memclearFuncs[libfunc_m] == memclear_libfunc);
    assert(memclearFuncs[nil_m]     == memclear_nil);
    assert(memclearFuncs[undef_m]   == memclear_undef);
}