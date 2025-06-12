#include "../headers/avm_memcell.h"
#include "../headers/avm_tables.h"
#include "../headers/avm.h"
#include <stdarg.h>

/* Type strings for debugging and typeof implementation */
char *typeStrings[] = {
    "number",
    "string", 
    "bool",
    "table",
    "userfunc",
    "libfunc",
    "nil",
    "undef"
};

/* Forward declaration for circular dependency */
extern void avm_tabledecrefcounter(avm_table *t);

/* Dispatch table for tostring conversions */
typedef char* (*tostring_func_t)(avm_memcell*);
tostring_func_t tostringFuncs[] = {
    number_tostring,
    string_tostring,
    bool_tostring,
    table_tostring,
    userfunc_tostring,
    libfunc_tostring,
    nil_tostring,
    undef_tostring
};

/* Dispatch table for tobool conversions */
typedef unsigned char (*tobool_func_t)(avm_memcell*);
tobool_func_t toboolFuncs[] = {
    number_tobool,
    string_tobool,
    bool_tobool,
    table_tobool,
    userfunc_tobool,
    libfunc_tobool,
    nil_tobool,
    undef_tobool
};

/* Memory cell cleanup - forward declaration */
extern void avm_memcellclear(avm_memcell *m);

/* Assign one memory cell to another */
void avm_assign(avm_memcell *lv, avm_memcell *rv) {
    if(lv == rv) return;
    
    if(lv->type == table_m && rv->type == table_m && lv->data.tableVal == rv->data.tableVal) return;
    if(rv->type == undef_m) avm_warning("assigning from 'undef' content!");
    
    avm_memcellclear(lv);
    
    memcpy(lv, rv, sizeof(avm_memcell));
    
    if(lv->type == string_m) lv->data.strVal = strdup(rv->data.strVal);
    else if(lv->type == table_m) avm_tableincrefcounter(lv->data.tableVal);
}

/* Convert memory cell to boolean value */
unsigned char avm_tobool(avm_memcell *m) {
    assert(m->type >= 0 && m->type <= undef_m);
    return (*toboolFuncs[m->type])(m);
}

/* Convert memory cell to string */
char *avm_tostring(avm_memcell *m) {
    assert(m->type >= 0 && m->type <= undef_m);
    return (*tostringFuncs[m->type])(m);
}

/* Type-specific string conversion functions */
char *number_tostring(avm_memcell *m) {
    char *s = (char*)malloc(32);
    sprintf(s, "%.3f", m->data.numVal);
    return s;
}

char *string_tostring(avm_memcell *m)   { return strdup(m->data.strVal); }
char *bool_tostring(avm_memcell *m)     { return strdup(m->data.boolVal ? "true" : "false"); }
char *table_tostring(avm_memcell *m)    { return strdup("table"); }
char *userfunc_tostring(avm_memcell *m) {
    char *s = (char*)malloc(64);
    sprintf(s, "userfunc@%u", m->data.funcVal);
    return s;
}
char *libfunc_tostring(avm_memcell *m)  { return strdup(m->data.libfuncVal); }
char *nil_tostring(avm_memcell *m)      { return strdup("nil"); }
char *undef_tostring(avm_memcell *m)    { return strdup("undef"); }

/* Type-specific boolean conversion functions */
unsigned char number_tobool(avm_memcell *m)     { return m->data.numVal != 0; }
unsigned char string_tobool(avm_memcell *m)     { return m->data.strVal[0] != '\0'; }
unsigned char bool_tobool(avm_memcell *m)       { return m->data.boolVal; }
unsigned char table_tobool(avm_memcell *m)      { return 1; }
unsigned char userfunc_tobool(avm_memcell *m)   { return 1; }
unsigned char libfunc_tobool(avm_memcell *m)    { return 1; }
unsigned char nil_tobool(avm_memcell *m)        { return 0; }
unsigned char undef_tobool(avm_memcell *m)      { return 0; }

/* Error and warning reporting */
void avm_warning(char *format, ...) {
    va_list args;
    va_start(args, format);
    
    fprintf(stderr, "Warning (line %u): ", vm.currLine);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}

void avm_error(char *format, ...) {
    va_list args;
    va_start(args, format);
    
    fprintf(stderr, "Error (line %u): ", vm.currLine);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    vm.executionFinished = 1;
    
    va_end(args);
}