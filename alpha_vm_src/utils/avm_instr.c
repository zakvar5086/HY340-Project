#include "../headers/avm_instr.h"
#include "../headers/avm_memcell.h"
#include "../headers/avm_tables.h"
#include "../headers/avm_libFunc.h"
#include "../headers/avm.h"

execute_func_t executeFuncs[] = {
    execute_assign,
    execute_add,
    execute_sub,
    execute_mul,
    execute_div,
    execute_mod,
    execute_uminus,
    execute_and,
    execute_or,
    execute_not,
    execute_jeq,
    execute_jne,
    execute_jle,
    execute_jge, 
    execute_jlt,
    execute_jgt,
    execute_jump,
    execute_call,
    execute_pusharg,
    execute_funcenter,
    execute_funcexit,
    execute_newtable,
    execute_tablegetelem,
    execute_tablesetelem,
    execute_nop  
};

/* Arithmetic function implementations */
double add_impl(double x, double y) { return x + y; }
double sub_impl(double x, double y) { return x - y; }
double mul_impl(double x, double y) { return x * y; }
double div_impl(double x, double y) { return x / y; /* Error check? */ }
double mod_impl(double x, double y) { return ((unsigned) x) % ((unsigned) y); /* Error check? */ }

/* Arithmetic dispatch table */
arithmetic_func_t arithmeticFuncs[] = {
    add_impl,
    sub_impl,
    mul_impl,
    div_impl,
    mod_impl
};

/* Comparison function implementations */
unsigned char jle_impl(double x, double y) { return x <= y; }
unsigned char jge_impl(double x, double y) { return x >= y; }
unsigned char jlt_impl(double x, double y) { return x < y; }
unsigned char jgt_impl(double x, double y) { return x > y; }

comparison_func_t comparisonFuncs[] = {
    jle_impl,
    jge_impl,
    jlt_impl,
    jgt_impl
};

void avm_initinstructions(void) {
    assert(sizeof(executeFuncs)/sizeof(execute_func_t) == 25);
    assert(sizeof(arithmeticFuncs)/sizeof(arithmetic_func_t) == 5);
    assert(sizeof(comparisonFuncs)/sizeof(comparison_func_t) == 4);
}

void execute_arithmetic(instruction *instr) {
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    avm_memcell *rv1 = avm_translate_operand(instr->arg1, &vm.ax);
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    
    assert(lv && (&vm.stack[vm.top] >= lv && lv > &vm.stack[vm.top] || lv == &vm.retval));
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

/* Assignment operation */
void execute_assign(instruction *instr) {
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    avm_memcell *rv = avm_translate_operand(instr->arg1, &vm.ax);
    
    assert(lv && (&vm.stack[vm.top] >= lv && lv > &vm.stack[vm.top] || lv == &vm.retval));
    assert(rv);
    
    avm_assign(lv, rv);
}

/* Arithmetic operations */
void execute_add(instruction *instr) { execute_arithmetic(instr); }
void execute_sub(instruction *instr) { execute_arithmetic(instr); }
void execute_mul(instruction *instr) { execute_arithmetic(instr); }
void execute_div(instruction *instr) { 
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    if(rv2->type == number_m && rv2->data.numVal == 0) {
        avm_error("division by zero!");
        vm.executionFinished = 1;
        return;
    }
    execute_arithmetic(instr); 
}
void execute_mod(instruction *instr) { 
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    if(rv2->type == number_m && ((unsigned)rv2->data.numVal) == 0) {
        avm_error("modulo by zero!");
        vm.executionFinished = 1;
        return;
    }
    execute_arithmetic(instr); 
}

void execute_uminus(instruction *instr) {
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    avm_memcell *rv = avm_translate_operand(instr->arg1, &vm.ax);
    
    assert(lv && (&vm.stack[vm.top] >= lv && lv > &vm.stack[vm.top] || lv == &vm.retval));
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

void execute_and(instruction *instr) {
    assert(0 && "execute_and should not be called: short-circuit front-end handles AND");
}

void execute_or(instruction *instr) {
    assert(0 && "execute_or should not be called: short-circuit front-end handles OR");
}

void execute_not(instruction *instr) {
    assert(0 && "execute_not should not be called: short-circuit front-end handles NOT");
}

/* Jump operations */
void execute_jump(instruction *instr) {
    assert(instr->result->type == label_a);
    vm.pc = instr->result->val;
}

/* Comparison and conditional jumps */
void execute_jeq(instruction *instr) {
    assert(instr->result->type == label_a);
    
    avm_memcell *rv1 = avm_translate_operand(instr->arg1, &vm.ax);
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    
    unsigned char result = 0;
    
    if(rv1->type == undef_m || rv2->type == undef_m) avm_error("'undef' involved in equality!");
    else if(rv1->type == nil_m || rv2->type == nil_m) result = rv1->type == nil_m && rv2->type == nil_m;
    else if(rv1->type == bool_m || rv2->type == bool_m) result = (avm_tobool(rv1) == avm_tobool(rv2));
    else if(rv1->type != rv2->type) avm_error("%s == %s is illegal!", typeStrings[rv1->type], typeStrings[rv2->type]);
    else {
        switch(rv1->type) {
            case number_m:
                result = (rv1->data.numVal == rv2->data.numVal);
                break;
            case string_m:
                result = (strcmp(rv1->data.strVal, rv2->data.strVal) == 0);
                break;
            default:
                assert(0);
        }
    }
    
    if(!vm.executionFinished && result) vm.pc = instr->result->val;
}

void execute_jne(instruction *instr) {
    assert(instr->result->type == label_a);
    
    avm_memcell *rv1 = avm_translate_operand(instr->arg1, &vm.ax);
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    
    unsigned char result = 0;
    
    if(rv1->type == undef_m || rv2->type == undef_m) avm_error("'undef' involved in equality!");
    else if(rv1->type == nil_m || rv2->type == nil_m) result = !(rv1->type == nil_m && rv2->type == nil_m);
    else if(rv1->type == bool_m || rv2->type == bool_m) result = (avm_tobool(rv1) != avm_tobool(rv2));
    else if(rv1->type != rv2->type) result = 1;
    else {
        switch(rv1->type) {
            case number_m:
                result = (rv1->data.numVal != rv2->data.numVal);
                break;
            case string_m:
                result = (strcmp(rv1->data.strVal, rv2->data.strVal) != 0);
                break;
            default:
                assert(0);
        }
    }
    
    if(!vm.executionFinished && result) vm.pc = instr->result->val;
}

void execute_jle(instruction *instr) {
    assert(instr->result->type == label_a);
    
    avm_memcell *rv1 = avm_translate_operand(instr->arg1, &vm.ax);
    avm_memcell *rv2 = avm_translate_operand(instr->arg2, &vm.bx);
    
    if(rv1->type != number_m || rv2->type != number_m) {
        avm_error("not a number in comparison!");
        vm.executionFinished = 1;
    } else {
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
            vm.pc = func->data.funcVal;
            assert(vm.pc < vm.codeSize);
            assert(vm.code[vm.pc].opcode == funcenter_v);
            break;
        }
        case string_m: avm_calllibfunc(func->data.strVal); break;
        case libfunc_m: avm_calllibfunc(func->data.libfuncVal); break;
        case table_m: avm_calllibfunc("call_functor"); break; /* Functors */
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
    
    avm_assign(&vm.stack[vm.top], arg);
    avm_dec_top();
}

void execute_funcenter(instruction *instr) {
    avm_memcell *func = avm_translate_operand(instr->result, &vm.ax);
    assert(func);
    assert(vm.pc == func->data.funcVal);
    
    /* Callee actions here. */
    userfunc_t *funcInfo = &vm.userfuncs[func->data.funcVal];
    vm.topsp = vm.top;
    vm.top = vm.top - funcInfo->localSize;
}

void execute_funcexit(instruction *instr) {
    unsigned oldTop = vm.top;
    vm.top = vm.stack[vm.topsp + AVM_SAVEDTOP_OFFSET].data.numVal;
    vm.pc = vm.stack[vm.topsp + AVM_SAVEDPC_OFFSET].data.numVal;
    vm.topsp = vm.stack[vm.topsp + AVM_SAVEDTOPSP_OFFSET].data.numVal;
    
    while(++oldTop <= vm.top) avm_memcellclear(&vm.stack[oldTop]);
}

void execute_newtable(instruction *instr) {
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    assert(lv && (&vm.stack[vm.top] >= lv && lv > &vm.stack[vm.top] || lv == &vm.retval));
    
    avm_memcellclear(lv);
    lv->type = table_m;
    lv->data.tableVal = avm_tablenew();
    avm_tableincrefcounter(lv->data.tableVal);
}

void execute_tablegetelem(instruction *instr) {
    avm_memcell *lv = avm_translate_operand(instr->result, (avm_memcell*) 0);
    avm_memcell *t = avm_translate_operand(instr->arg1, (avm_memcell*) 0);
    avm_memcell *i = avm_translate_operand(instr->arg2, &vm.ax);
    
    assert(lv && (&vm.stack[vm.top] >= lv && lv > &vm.stack[vm.top] || lv == &vm.retval));
    assert(t && (&vm.stack[vm.top] >= t && t > &vm.stack[vm.top]));
    assert(i);
    
    avm_memcellclear(lv);
    lv->type = nil_m;
    
    if(t->type != table_m) avm_error("illegal use of type %s as table!", typeStrings[t->type]);
    else {
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
    
    assert(t && (&vm.stack[vm.top] >= t && t > &vm.stack[vm.top]));
    assert(i && c);
    
    if(t->type != table_m) avm_error("illegal use of type %s as table!", typeStrings[t->type]);
    else avm_tablesetelem(t->data.tableVal, i, c);
}

void execute_nop(instruction *instr) {  }