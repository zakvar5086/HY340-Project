all: alpha_parser

alpha_parser: lex.yy.c parser.tab.c symtablehash.c quad.c
	gcc -g -o alpha_parser lex.yy.c parser.tab.c symtablehash.c quad.c -ll

parser.tab.c parser.tab.h: parser.y
	bison -d parser.y

lex.yy.c: lex.l parser.tab.h
	flex lex.l

clean:
	rm -f alpha_parser lex.yy.c parser.tab.* *.o