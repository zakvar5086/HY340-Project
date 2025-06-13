#include "../headers/avm.h"

avm_state vm;

void avm_initialize(void) {
    avm_initgc();
    avm_initinstructions();
    avm_initlibraryfuncs();
    avm_init_env_stack();
    
    memset(&vm.retval, 0, sizeof(avm_memcell));
    memset(&vm.ax, 0, sizeof(avm_memcell));
    memset(&vm.bx, 0, sizeof(avm_memcell));
    memset(&vm.cx, 0, sizeof(avm_memcell));
    
    vm.retval.type = nil_m;
    vm.ax.type = undef_m;
    vm.bx.type = undef_m;
    vm.cx.type = undef_m;
    
    vm.pc = 1;
    vm.currLine = 0;
    vm.executionFinished = 0;
    vm.codeSize = 0;
    vm.code = NULL;
    vm.programVarCount = 0;
    
    vm.strings = NULL;
    vm.numbers = NULL;
    vm.libfuncs = NULL;
    vm.userfuncs = NULL;
    vm.totalStrings = 0;
    vm.totalNumbers = 0;
    vm.totalLibfuncs = 0;
    vm.totalUserfuncs = 0;
    
    vm.stack = NULL;
    vm.top = 0;
    vm.topsp = 0;
}

void avm_run(char *filename) {
    FILE *file = fopen(filename, "rb");
    if(!file) {
        fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
        return;
    }
    
    avm_load_program(file);
    fclose(file);
    
    while(!vm.executionFinished) execute_cycle();
}

void avm_cleanup(void) {
    unsigned i;
    
    if(vm.stack) {
        for(i = 0; i < AVM_STACKSIZE; i++) avm_memcellclear(&vm.stack[i]);
        free(vm.stack);
        vm.stack = NULL;
    }
    
    avm_memcellclear(&vm.retval);
    avm_memcellclear(&vm.ax);
    avm_memcellclear(&vm.bx);
    avm_memcellclear(&vm.cx);
    avm_cleanup_env_stack();
    
    if(vm.strings) {
        for(i = 0; i < vm.totalStrings; i++)
            if(vm.strings[i]) free(vm.strings[i]);
        free(vm.strings);
        vm.strings = NULL;
    }
    
    if(vm.numbers) {
        free(vm.numbers);
        vm.numbers = NULL;
    }
    
    if(vm.libfuncs) {
        for(i = 0; i < vm.totalLibfuncs; i++)
            if(vm.libfuncs[i]) free(vm.libfuncs[i]);
        free(vm.libfuncs);
        vm.libfuncs = NULL;
    }
    
    if(vm.userfuncs) {
        for(i = 0; i < vm.totalUserfuncs; i++)
            if(vm.userfuncs[i].id) free(vm.userfuncs[i].id);
        free(vm.userfuncs);
        vm.userfuncs = NULL;
    }
    
    if(vm.code) {
        free(vm.code);
        vm.code = NULL;
    }
}

void avm_initstack(void) {
    vm.stack = (avm_memcell*)malloc((AVM_STACKSIZE + 1) * sizeof(avm_memcell));
    if(!vm.stack) {
        fprintf(stderr, "Error: Failed to allocate VM stack\n");
        exit(1);
    }
    
    unsigned i;
    for(i = 0; i <= AVM_STACKSIZE; i++) {
        vm.stack[i].type = undef_m;
        memset(&vm.stack[i].data, 0, sizeof(vm.stack[i].data));
    }

    vm.top = AVM_STACKSIZE - vm.programVarCount;
    vm.topsp = vm.top;
    
    printf("Stack initialized: topsp=%u, top=%u, programVarCount=%u\n", 
           vm.topsp, vm.top, vm.programVarCount);
}

void avm_dec_top(void) {
    if(vm.top < 1) {
        avm_error("stack overflow");
        vm.executionFinished = 1;
    } else {
        --vm.top;
    }
}

void avm_push_envvalue(unsigned val) {
    vm.stack[vm.top].type = number_m;
    vm.stack[vm.top].data.numVal = val;
    avm_dec_top();
}

void avm_callsaveenvironment(void) {
    avm_push_envvalue(vm.totalActuals);
    assert(vm.code[vm.pc].opcode == call_v);
    avm_push_envvalue(vm.pc + 1);
    avm_push_envvalue(vm.top + vm.totalActuals + 2);    
    avm_push_envvalue(vm.topsp);    
}

void avm_load_program(FILE *file) {
    unsigned magicNumber;
    if(fread(&magicNumber, sizeof(unsigned), 1, file) != 1 || 
        magicNumber != AVM_MAGICNUMBER) {
        fprintf(stderr, "Error: Invalid binary file format\n");
        exit(1);
    }
    
    // Read global variable count
    if(fread(&vm.programVarCount, sizeof(unsigned), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read global variable count\n");
        exit(1);
    }
    
    avm_load_constants(file);
    avm_load_code(file);
    
    avm_initstack();
}

void avm_load_constants(FILE *file) {
    unsigned i;
    
    if(fread(&vm.totalStrings, sizeof(unsigned), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read string count\n");
        exit(1);
    }
    
    if(vm.totalStrings > 0) {
        vm.strings = (char**)malloc(vm.totalStrings * sizeof(char*));
        for(i = 0; i < vm.totalStrings; i++) {
            unsigned len;
            if(fread(&len, sizeof(unsigned), 1, file) != 1) {
                fprintf(stderr, "Error: Failed to read string length\n");
                exit(1);
            }
            vm.strings[i] = (char*)malloc(len + 1);
            if(fread(vm.strings[i], sizeof(char), len, file) != len) {
                fprintf(stderr, "Error: Failed to read string data\n");
                exit(1);
            }
            vm.strings[i][len] = '\0';
        }
    }
    
    if(fread(&vm.totalNumbers, sizeof(unsigned), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read number count\n");
        exit(1);
    }
    if(vm.totalNumbers > 0) {
        vm.numbers = (double*)malloc(vm.totalNumbers * sizeof(double));
        if(fread(vm.numbers, sizeof(double), vm.totalNumbers, file) != vm.totalNumbers) {
            fprintf(stderr, "Error: Failed to read number data\n");
            exit(1);
        }
    }
    
    if(fread(&vm.totalLibfuncs, sizeof(unsigned), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read libfunc count\n");
        exit(1);
    }
    if(vm.totalLibfuncs > 0) {
        vm.libfuncs = (char**)malloc(vm.totalLibfuncs * sizeof(char*));
        for(i = 0; i < vm.totalLibfuncs; i++) {
            unsigned len;
            if(fread(&len, sizeof(unsigned), 1, file) != 1) {
                fprintf(stderr, "Error: Failed to read libfunc name length\n");
                exit(1);
            }
            
            vm.libfuncs[i] = (char*)malloc(len + 1);
            if(fread(vm.libfuncs[i], sizeof(char), len, file) != len) {
                fprintf(stderr, "Error: Failed to read libfunc name\n");
                exit(1);
            }
            vm.libfuncs[i][len] = '\0';
        }
    }
    
    if(fread(&vm.totalUserfuncs, sizeof(unsigned), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read userfunc count\n");
        exit(1);
    }
    
    if(vm.totalUserfuncs > 0) {
        vm.userfuncs = (userfunc_t*)malloc(vm.totalUserfuncs * sizeof(userfunc_t));
        for(i = 0; i < vm.totalUserfuncs; i++) {
            if(fread(&vm.userfuncs[i].address, sizeof(unsigned), 1, file) != 1 ||
                fread(&vm.userfuncs[i].localSize, sizeof(unsigned), 1, file) != 1 ||
                fread(&vm.userfuncs[i].saved_index, sizeof(unsigned), 1, file) != 1) {
                fprintf(stderr, "Error: Failed to read userfunc data\n");
                exit(1);
            }
            
            unsigned len;
            if(fread(&len, sizeof(unsigned), 1, file) != 1) {
                fprintf(stderr, "Error: Failed to read userfunc name length\n");
                exit(1);
            }
            vm.userfuncs[i].id = (char*)malloc(len + 1);
            if(fread(vm.userfuncs[i].id, sizeof(char), len, file) != len) {
                fprintf(stderr, "Error: Failed to read userfunc name\n");
                exit(1);
            }
            vm.userfuncs[i].id[len] = '\0';
        }
    }
}

void avm_load_code(FILE *file) {    
    if (fread(&vm.codeSize, sizeof(unsigned), 1, file) != 1) {
        fprintf(stderr, "Error: Failed to read code size\n");
        exit(1);
    }
    
    if (vm.codeSize > 0) {
        vm.code = (instruction*)malloc((vm.codeSize + 1) * sizeof(instruction));
        
        unsigned i;
        for (i = 0; i < vm.codeSize; i++) {
            instruction *instr = &vm.code[i + 1];
            
            if (fread(&instr->opcode, sizeof(vmopcode), 1, file) != 1 ||
                fread(&instr->srcLine, sizeof(unsigned), 1, file) != 1) {
                fprintf(stderr, "Error: Failed to read instruction header for instruction %u\n", i);
                exit(1);
            }
                        
            instr->result = (vmarg*)malloc(sizeof(vmarg));
            instr->arg1 = (vmarg*)malloc(sizeof(vmarg));
            instr->arg2 = (vmarg*)malloc(sizeof(vmarg));
            
            if (fread(instr->result, sizeof(vmarg), 1, file) != 1 ||
                fread(instr->arg1, sizeof(vmarg), 1, file) != 1 ||
                fread(instr->arg2, sizeof(vmarg), 1, file) != 1) {
                fprintf(stderr, "Error: Failed to read instruction operands for instruction %u\n", i);
                exit(1);
            }
        }
    }
}

void execute_cycle(void) {
    if(vm.executionFinished) return;
    
    if(vm.pc < 1 || vm.pc > vm.codeSize) {
        fprintf(stderr, "Error: PC %u out of bounds (1-%u)\n", vm.pc, vm.codeSize);
        vm.executionFinished = 1;
        return;
    }
    
    instruction *instr = vm.code + vm.pc;
    
    if(instr->opcode < 0 || instr->opcode > AVM_MAX_INSTRUCTIONS) {
        fprintf(stderr, "Error: Invalid opcode %d at instruction %u\n", instr->opcode, vm.pc);
        vm.executionFinished = 1;
        return;
    }
    
    if(instr->srcLine) vm.currLine = instr->srcLine;
    
    unsigned oldPC = vm.pc;
    (*executeFuncs[instr->opcode])(instr);
        
    if(vm.pc == oldPC) ++vm.pc;
}

avm_memcell *avm_translate_operand(vmarg *arg, avm_memcell *reg) {
    switch(arg->type) {
        case global_a: {
            unsigned addr = AVM_STACKSIZE  - arg->val;
            return &vm.stack[addr];
        }
        case local_a: {
            unsigned addr = vm.topsp - arg->val;
            if(addr < 1 || addr > AVM_STACKSIZE) {
                avm_error("local address %u out of bounds", addr);
                return NULL;
            }
            return &vm.stack[addr];
        }
        case formal_a: {
            unsigned addr = vm.topsp + AVM_STACKENV_SIZE + 1 + arg->val;
            if(addr < 1 || addr > AVM_STACKSIZE) {
                avm_error("formal address %u out of bounds", addr);
                return NULL;
            }
            return &vm.stack[addr];
        }
        case retval_a:
            return &vm.retval;
        case number_a: {
            assert(arg->val < vm.totalNumbers);
            reg->type = number_m;
            reg->data.numVal = vm.numbers[arg->val];
            return reg;
        }
        case string_a: {
            assert(arg->val < vm.totalStrings);
            reg->type = string_m;
            reg->data.strVal = strdup(vm.strings[arg->val]);
            return reg;
        }
        case bool_a: {
            reg->type = bool_m;
            reg->data.boolVal = arg->val;
            return reg;
        }
        case nil_a: {
            reg->type = nil_m;
            return reg;
        }
        case userfunc_a: {
            assert(arg->val < vm.totalUserfuncs);
            reg->type = userfunc_m;
            reg->data.funcVal = arg->val;
            return reg;
        }
        case libfunc_a: {
            assert(arg->val < vm.totalLibfuncs);
            reg->type = libfunc_m;
            reg->data.libfuncVal = vm.libfuncs[arg->val];
            return reg;
        }
        default: 
            printf("ERROR: Unknown arg type %d\n", arg->type);
            avm_error("Unknown argument type %d", arg->type);
            assert(0);
    }
}

unsigned avm_get_envvalue(unsigned i) {
    if(i < 1 || i > AVM_STACKSIZE) {
        avm_error("Environment value index %u out of bounds (valid range: 1-%u)", i, AVM_STACKSIZE);
        return 0;
    }
    
    if(vm.stack[i].type != number_m) {
        avm_error("Environment value at index %u is not a number (type: %d)", i, vm.stack[i].type);
        return 0;
    }
    
    unsigned val = (unsigned) vm.stack[i].data.numVal;
    if(vm.stack[i].data.numVal != ((double) val)) {
        avm_error("Environment value at index %u is not an integer", i);
        return 0;
    }
    
    return val;
}

userfunc_t *avm_getfuncinfo(unsigned address) {
    for(unsigned i = 0; i < vm.totalUserfuncs; i++) {
        if(vm.userfuncs[i].address == address) {
            return &vm.userfuncs[i];
        }
    }
    return NULL;
}

void avm_push_table_arg(avm_table *t) {
    vm.stack[vm.top].type = table_m;
    avm_tableincrefcounter(vm.stack[vm.top].data.tableVal = t);
    ++vm.totalActuals;
    avm_dec_top();
}

void avm_call_functor(avm_table *t) {
    vm.cx.type = string_m;
    vm.cx.data.strVal = "()";
    avm_memcell *f = avm_tablegetelem(t, &vm.cx);
    
    if(!f) {
        avm_error("in calling table: no '()' element found!");
    } else {
        if(f->type == table_m) {
            avm_call_functor(f->data.tableVal);
        } else if(f->type == userfunc_m) {
            avm_push_table_arg(t);
            avm_callsaveenvironment();
            vm.pc = f->data.funcVal;
            assert(vm.pc < vm.codeSize + 1 && vm.code[vm.pc].opcode == funcenter_v);
        } else {
            avm_error("in calling table: illegal '()' element value!");
        }
    }
}

void debug_print_stack_state(const char* context) {
    printf("=== Stack State (%s) ===\n", context);
    printf("topsp: %u, top: %u, totalActuals: %u\n", vm.topsp, vm.top, vm.totalActuals);
    printf("Stack contents around top:\n");
    
    unsigned start = (vm.top > 5) ? vm.top - 5 : 1;
    unsigned end = (vm.top + 10 < AVM_STACKSIZE) ? vm.top + 10 : AVM_STACKSIZE;
    
    for(unsigned i = start; i <= end; i++) {
        char marker = ' ';
        if(i == vm.top) marker = 'T';
        else if(i == vm.topsp) marker = 'S';
        
        printf("[%u]%c type:%d ", i, marker, vm.stack[i].type);
        if(vm.stack[i].type == number_m) {
            printf("val:%.0f", vm.stack[i].data.numVal);
        } else if(vm.stack[i].type == string_m && vm.stack[i].data.strVal) {
            printf("val:\"%s\"", vm.stack[i].data.strVal);
        }
        printf("\n");
    }
    printf("========================\n");
}