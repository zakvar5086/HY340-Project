#ifndef AVM_MEMCELL_H
#define AVM_MEMCELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Forward declarations */
struct avm_table;

/* Memory cell types */
typedef enum avm_memcell_t {
    number_m    = 0,
    string_m    = 1,
    bool_m      = 2,
    table_m     = 3,
    userfunc_m  = 4,
    libfunc_m   = 5,
    nil_m       = 6,
    undef_m     = 7
} avm_memcell_t;

/* Memory cell structure - core data type of the VM */
typedef struct avm_memcell {
    avm_memcell_t type;
    union {
        double numVal;
        char *strVal;
        unsigned char boolVal;
        struct avm_table *tableVal;
        unsigned funcVal;        /* For user functions (index) */
        char *libfuncVal;        /* For library functions (name) */
    } data;
} avm_memcell;

/* Memory cell operations */
void avm_memcellclear(avm_memcell *m);
void avm_assign(avm_memcell *lv, avm_memcell *rv);

/* Type checking and conversion */
unsigned char avm_tobool(avm_memcell *m);
char *avm_tostring(avm_memcell *m);

/* Type-specific string conversion functions */
char *number_tostring(avm_memcell *m);
char *string_tostring(avm_memcell *m);
char *bool_tostring(avm_memcell *m);
char *table_tostring(avm_memcell *m);
char *userfunc_tostring(avm_memcell *m);
char *libfunc_tostring(avm_memcell *m);
char *nil_tostring(avm_memcell *m);
char *undef_tostring(avm_memcell *m);

/* Type-specific boolean conversion functions */
unsigned char number_tobool(avm_memcell *m);
unsigned char string_tobool(avm_memcell *m);
unsigned char bool_tobool(avm_memcell *m);
unsigned char table_tobool(avm_memcell *m);
unsigned char userfunc_tobool(avm_memcell *m);
unsigned char libfunc_tobool(avm_memcell *m);
unsigned char nil_tobool(avm_memcell *m);
unsigned char undef_tobool(avm_memcell *m);

/* Utility functions */
void avm_error(char *format, ...);
void avm_warning(char *format, ...);

/* Type strings for debugging */
extern char *typeStrings[];

#endif