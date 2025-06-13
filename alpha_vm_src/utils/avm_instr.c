#include "../headers/avm_instr.h"
#include "../headers/avm_memcell.h"
#include "../headers/avm_tables.h"
#include "../headers/avm_libFunc.h"
#include "../headers/avm.h"
#include "../../alpha_parser_src/headers/stack.h"

execute_func_t executeFuncs[] = {
    execute_assign, execute_add, execute_sub, execute_mul, execute_div, execute_mod,
    execute_uminus, execute_and, execute_or, execute_not, execute_jeq, execute_jne,
    execute_jle, execute_jge, execute_jlt, execute_jgt, execute_jump, execute_call,
    execute_pusharg, execute_funcenter, execute_funcexit, execute_newtable,
    execute_tablegetelem, execute_tablesetelem, execute_nop  
};

double add_impl(double x, double y) { return x + y; }
double sub_impl(double x, double y) { return x - y; }
double mul_impl(double x, double y) { return x * y; }
double div_impl(double x, double y) { if (y == 0.0) { avm_error("Division by zero!"); return 0.0; } return x / y; }
double mod_impl(double x, double y) { if (((unsigned) y) == 0) { avm_error("Modulo by zero!"); return 0.0; } return ((unsigned) x) % ((unsigned) y); }

arithmetic_func_t arithmeticFuncs[] = { add_impl, sub_impl, mul_impl, div_impl, mod_impl };

unsigned char jle_impl(double x, double y) { return x <= y; }
unsigned char jge_impl(double x, double y) { return x >= y; }
unsigned char jlt_impl(double x, double y) { return x < y; }
unsigned char jgt_impl(double x, double y) { return x > y; }

comparison_func_t comparisonFuncs[] = { jle_impl, jge_impl, jlt_impl, jgt_impl };

void avm_initinstructions(void) {
    assert(sizeof(executeFuncs)/sizeof(execute_func_t) == 25);
    assert(sizeof(arithmeticFuncs)/sizeof(arithmetic_func_t) == 5);
    assert(sizeof(comparisonFuncs)/sizeof(comparison_func_t) == 4);
}

void execute_arithmetic(instruction *instr) {
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    avm_memcell *rv1 = avm_translate_operand(instr->arg1, &vm.ax);
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    
    assert(lv && ((lv >= &vm.stack[1] && lv <= &vm.stack[AVM_STACKSIZE]) || lv == &vm.retval));
    assert(rv1 && rv2);

    if(rv1->type != number_m || rv2->type != number_m) {
        avm_error("not a number in arithmetic!");
        vm.executionFinished = 1;
    } else {
        arithmetic_func_t op = arithmeticFuncs[instr->opcode - add_v];
        avm_memcellclear(lv);
        lv->type = number_m;
        lv->data.numVal = (*op)(rv1->data.numVal, rv2->data.numVal);
    }
}

void execute_assign(instruction *instr) {
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    avm_memcell *rv = avm_translate_operand(instr->arg1, &vm.ax);
        
    assert(lv && ((lv >= &vm.stack[1] && lv <= &vm.stack[AVM_STACKSIZE]) || lv == &vm.retval));
    assert(rv);
    
    avm_assign(lv, rv);
}

void execute_add(instruction *instr) { execute_arithmetic(instr); }
void execute_sub(instruction *instr) { execute_arithmetic(instr); }
void execute_mul(instruction *instr) { execute_arithmetic(instr); }
void execute_div(instruction *instr) { execute_arithmetic(instr); }
void execute_mod(instruction *instr) { execute_arithmetic(instr); }

void execute_uminus(instruction *instr) {
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    avm_memcell *rv = avm_translate_operand(instr->arg1, &vm.ax);
    
    assert(lv && ((lv >= &vm.stack[1] && lv <= &vm.stack[AVM_STACKSIZE]) || lv == &vm.retval));
    assert(rv);
    
    if(rv->type != number_m) {
        avm_error("not a number in unary minus!");
        vm.executionFinished = 1;
    } else {
        avm_memcellclear(lv);
        lv->type = number_m;
        lv->data.numVal = -rv->data.numVal;
    }
}

void execute_and(instruction *instr) { assert(0 && "execute_and should not be called"); }
void execute_or(instruction *instr) { assert(0 && "execute_or should not be called"); }
void execute_not(instruction *instr) { assert(0 && "execute_not should not be called"); }

void execute_jump(instruction *instr) {
    assert(instr->result->type == label_a);
    vm.pc = instr->result->val;
}

typedef unsigned char (*equality_func_t)(avm_memcell*, avm_memcell*);
unsigned char eq_number(avm_memcell *rv1, avm_memcell *rv2) { return rv1->data.numVal == rv2->data.numVal; }
unsigned char eq_string(avm_memcell *rv1, avm_memcell *rv2) { return strcmp(rv1->data.strVal, rv2->data.strVal) == 0; }
unsigned char eq_bool(avm_memcell *rv1, avm_memcell *rv2) { return rv1->data.boolVal == rv2->data.boolVal; }
unsigned char eq_table(avm_memcell *rv1, avm_memcell *rv2) { return rv1->data.tableVal == rv2->data.tableVal; }
unsigned char eq_userfunc(avm_memcell *rv1, avm_memcell *rv2) { return rv1->data.funcVal == rv2->data.funcVal; }
unsigned char eq_libfunc(avm_memcell *rv1, avm_memcell *rv2) { return strcmp(rv1->data.libfuncVal, rv2->data.libfuncVal) == 0; }
unsigned char eq_nil(avm_memcell *rv1, avm_memcell *rv2) { return 1; }
unsigned char eq_undef(avm_memcell *rv1, avm_memcell *rv2) { avm_error("'undef' involved in equality!"); return 0; }

equality_func_t equalityFuncs[] = { eq_number, eq_string, eq_bool, eq_table, eq_userfunc, eq_libfunc, eq_nil, eq_undef };

void execute_jeq(instruction *instr) {
    assert(instr->result->type == label_a);
    avm_memcell *rv1 = avm_translate_operand(instr->arg1, &vm.ax);
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    unsigned char result = 0;
    if(rv1->type == undef_m || rv2->type == undef_m) { avm_error("'undef' involved in equality!"); } 
    else if(rv1->type == nil_m || rv2->type == nil_m) { result = rv1->type == nil_m && rv2->type == nil_m; } 
    else if(rv1->type == bool_m || rv2->type == bool_m) { result = (avm_tobool(rv1) == avm_tobool(rv2)); } 
    else if(rv1->type != rv2->type) { avm_error("%s == %s is illegal!", typeStrings[rv1->type], typeStrings[rv2->type]); } 
    else { result = (*equalityFuncs[rv1->type])(rv1, rv2); }
    if(!vm.executionFinished && result) vm.pc = instr->result->val;
}

void execute_jne(instruction *instr) {
    assert(instr->result->type == label_a);
    avm_memcell *rv1 = avm_translate_operand(instr->arg1, &vm.ax);
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    unsigned char result = 0;
    if(rv1->type == undef_m || rv2->type == undef_m) { avm_error("'undef' involved in equality!"); } 
    else if(rv1->type == nil_m || rv2->type == nil_m) { result = !(rv1->type == nil_m && rv2->type == nil_m); } 
    else if(rv1->type == bool_m || rv2->type == bool_m) { result = (avm_tobool(rv1) != avm_tobool(rv2)); } 
    else if(rv1->type != rv2->type) { result = 1; } 
    else { result = !(*equalityFuncs[rv1->type])(rv1, rv2); }
    if(!vm.executionFinished && result) vm.pc = instr->result->val;
}

void execute_jle(instruction *instr) {
    assert(instr->result->type == label_a);
    avm_memcell *rv1 = avm_translate_operand(instr->arg1, &vm.ax);
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    if(rv1->type != number_m || rv2->type != number_m) { avm_error("not a number in comparison!"); } 
    else {
        comparison_func_t op = comparisonFuncs[instr->opcode - jle_v];
        if((*op)(rv1->data.numVal, rv2->data.numVal)) vm.pc = instr->result->val;
    }
}

void execute_jge(instruction *instr) { execute_jle(instr); }
void execute_jlt(instruction *instr) { execute_jle(instr); }
void execute_jgt(instruction *instr) { execute_jle(instr); }

void execute_call(instruction *instr) {
    avm_memcell *func = avm_translate_operand(instr->arg1, &vm.ax);
    assert(func);
        
    switch(func->type) {
        case userfunc_m: {
            avm_callsaveenvironment();
            vm.pc = vm.userfuncs[func->data.funcVal].address;
            assert(vm.pc <= vm.codeSize);
            assert(vm.code[vm.pc].opcode == funcenter_v);
            break;
        }
        
        case string_m: 
        case libfunc_m: {
            char *funcName = (func->type == string_m) ? func->data.strVal : func->data.libfuncVal;
            library_func_t f = avm_getlibraryfunc(funcName);
            
            if(!f) {
                avm_error("unsupported lib func '%s' called!", funcName);
                vm.executionFinished = 1;
            } else {
                unsigned total_args_to_clean = vm.totalActuals;
                (*f)();
                
                if (!vm.executionFinished) {
                    for (unsigned i = 0; i < total_args_to_clean; ++i) {
                        avm_memcellclear(&vm.stack[vm.top + 1 + i]);
                    }
                    vm.top += total_args_to_clean;
                }
            }
            break;
        }

        case table_m: {
            avm_call_functor(func->data.tableVal); 
            break;
        }

        default: {
            char *s = avm_tostring(func);
            avm_error("call: cannot bind '%s' to function!", s);
            free(s);
            vm.executionFinished = 1;
        }
    }
}

void execute_pusharg(instruction *instr) {
    avm_memcell *arg = avm_translate_operand(instr->arg1, &vm.ax);
    assert(arg);
    
    if (vm.pc > 1 && vm.code[vm.pc-1].opcode != pusharg_v) {
        vm.totalActuals = 0;
    } else if (vm.pc == 1) {
        vm.totalActuals = 0;
    }
    
    avm_assign(&vm.stack[vm.top], arg);
    ++vm.totalActuals;
    avm_dec_top();    
}

void execute_funcenter(instruction *instr) {
    avm_memcell *func = avm_translate_operand(instr->result, &vm.ax);
    assert(func);
    assert(vm.pc == vm.userfuncs[func->data.funcVal].address);
    
    userfunc_t *funcInfo = avm_getfuncinfo(vm.pc);
    vm.topsp = vm.top;
    vm.top = vm.top - funcInfo->localSize;
    
    vm.totalActuals = 0;
}

void execute_funcexit(instruction *unused) {
    unsigned oldTop = vm.top;
    vm.top = avm_get_envvalue(vm.topsp + AVM_SAVEDTOP_OFFSET);
    vm.pc = avm_get_envvalue(vm.topsp + AVM_SAVEDPC_OFFSET);
    vm.topsp = avm_get_envvalue(vm.topsp + AVM_SAVEDTOPSP_OFFSET);
    
    while(++oldTop <= vm.top)
        avm_memcellclear(&vm.stack[oldTop]);
}

void execute_newtable(instruction *instr) {  
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    assert(lv && ((lv >= &vm.stack[1] && lv <= &vm.stack[AVM_STACKSIZE]) || lv == &vm.retval));

    avm_memcellclear(lv);
    lv->type = table_m;
    lv->data.tableVal = avm_tablenew();
    avm_tableincrefcounter(lv->data.tableVal);
}

void execute_tablegetelem(instruction *instr) {
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    avm_memcell *t = avm_translate_operand(instr->arg1, (avm_memcell*) 0);
    avm_memcell *i = avm_translate_operand(instr->arg2, &vm.ax);
    
    assert(lv && ((lv >= &vm.stack[1] && lv <= &vm.stack[AVM_STACKSIZE]) || lv == &vm.retval));
    assert(t && (t >= &vm.stack[1] && t <= &vm.stack[AVM_STACKSIZE]));
    assert(i);
    
    avm_memcellclear(lv);
    lv->type = nil_m;
    
    if(t->type != table_m) {  
        avm_error("illegal use of type %s as table!", typeStrings[t->type]);
    } else {
        avm_memcell *content = avm_tablegetelem(t->data.tableVal, i);
        if(content) avm_assign(lv, content);
        else {
            char *ts = avm_tostring(t);
            char *is = avm_tostring(i);
            avm_warning("%s[%s] not found!", ts, is);
            free(ts);
            free(is);
        }
    }
}

void execute_tablesetelem(instruction *instr) {
    avm_memcell *t = avm_translate_operand(instr->result, (avm_memcell*) 0);
    avm_memcell *i = avm_translate_operand(instr->arg1, &vm.ax);
    avm_memcell *c = avm_translate_operand(instr->arg2, &vm.bx);
    
    assert(t && (t >= &vm.stack[1] && t <= &vm.stack[AVM_STACKSIZE]));
    assert(i && c);
    
    if(t->type != table_m) {
        avm_error("illegal use of type %s as table!", typeStrings[t->type]);
    } else {
        avm_tablesetelem(t->data.tableVal, i, c);
    }
}

void execute_nop(instruction *instr) { /* Empty */ }

/* Stack for saving env base addr */
void avm_init_env_stack(void) {
    env_base_stack = newStack();
    if (!env_base_stack) {
        fprintf(stderr, "Error: Failed to initialize environment base stack\n");
        exit(1);
    }
}

void avm_cleanup_env_stack(void) {
    if (env_base_stack) {
        while (!isEmptyStack(env_base_stack)) {
            unsigned *base = (unsigned*)popStack(env_base_stack);
            if (base) free(base);
        }
        destroyStack(env_base_stack);
        env_base_stack = NULL;
    }
}

void avm_push_env_base(unsigned base) {
    unsigned *base_ptr = malloc(sizeof(unsigned));
    if (!base_ptr) {
        avm_error("Failed to allocate memory for environment base");
        vm.executionFinished = 1;
        return;
    }
    *base_ptr = base;
    pushStack(env_base_stack, base_ptr);
}

unsigned avm_pop_env_base(void) {
    if (isEmptyStack(env_base_stack)) {
        avm_error("Environment base stack is empty");
        vm.executionFinished = 1;
        return 0;
    }
    
    unsigned *base_ptr = (unsigned*)popStack(env_base_stack);
    unsigned base = *base_ptr;
    free(base_ptr);
    return base;
}

unsigned avm_peek_env_base(void) {
    if (isEmptyStack(env_base_stack)) {
        avm_error("Environment base stack is empty");
        vm.executionFinished = 1;
        return 0;
    }
    
    unsigned *base_ptr = (unsigned*)topStack(env_base_stack);
    return *base_ptr;
}