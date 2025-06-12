all: alpha_parser avm

alpha_parser: alpha_parser_src/utils/symtablehash.o alpha_parser_src/utils/quad.o alpha_parser_src/utils/stack.o alpha_parser_src/utils/targetcode.o alpha_parser_src/parser.tab.o lex.yy.o
	gcc -g -Wall -Ialpha_parser_src/headers -Ialpha_vm_src/headers -o alpha_parser alpha_parser_src/utils/symtablehash.o alpha_parser_src/utils/quad.o alpha_parser_src/utils/stack.o alpha_parser_src/utils/targetcode.o alpha_parser_src/parser.tab.o lex.yy.o -ll

avm: alpha_vm_src/utils/avm.o alpha_vm_src/utils/avm_memcell.o alpha_vm_src/utils/avm_tables.o alpha_vm_src/utils/avm_gc.o alpha_vm_src/utils/avm_instr.o alpha_vm_src/utils/avm_libFunc.o main.o
	gcc -g -Wall -Ialpha_parser_src/headers -Ialpha_vm_src/headers -o avm alpha_vm_src/utils/avm.o alpha_vm_src/utils/avm_memcell.o alpha_vm_src/utils/avm_tables.o alpha_vm_src/utils/avm_gc.o alpha_vm_src/utils/avm_instr.o alpha_vm_src/utils/avm_libFunc.o alpha_parser_src/utils/stack.o main.o -lm
alpha_parser_src/parser.tab.c alpha_parser_src/parser.tab.h: alpha_parser_src/parser.y
	cd alpha_parser_src && bison -d parser.y

lex.yy.c: alpha_parser_src/lex.l alpha_parser_src/parser.tab.h
	cd alpha_parser_src && flex lex.l
	mv alpha_parser_src/lex.yy.c .

%.o: %.c
	gcc -g -Wall -Ialpha_parser_src/headers -Ialpha_vm_src/headers -c $< -o $@

clean:
	rm -f alpha_parser avm alpha_parser_src/parser.tab.c lex.yy.c alpha_parser_src/utils/symtablehash.o alpha_parser_src/utils/quad.o alpha_parser_src/utils/stack.o alpha_parser_src/utils/targetcode.o alpha_parser_src/parser.tab.o lex.yy.o alpha_vm_src/utils/avm.o alpha_vm_src/utils/avm_memcell.o alpha_vm_src/utils/avm_tables.o alpha_vm_src/utils/avm_gc.o alpha_vm_src/utils/avm_instr.o alpha_vm_src/utils/avm_libFunc.o main.o
	rm -f alpha_parser_src/parser.tab.h
	rm -f test.abc
	clear