all: alpha_parser

alpha_parser: lex.yy.c parser.tab.c utils/symtablehash.c utils/quad.c utils/targetcode.c
	gcc -g -o alpha_parser lex.yy.c parser.tab.c utils/symtablehash.c utils/quad.c utils/stack.c utils/targetcode.c -ll -g

parser.tab.c parser.tab.h: parser.y
	bison -d parser.y

lex.yy.c: lex.l parser.tab.h
	flex lex.l

clean:
	rm -f alpha_parser lex.yy.c parser.tab.* *.o
	clear
	@echo "All files cleaned."

clear:
	$(MAKE) clean
	$(MAKE)
	clear
	@echo "All files cleaned and recompiled."
