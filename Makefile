CC      = gcc
FLEX    = flex

CFLAGS  = -Wall -g -Wno-unused-function

LEX_FILE = lex.l
C_FILE   = lex.yy.c
EXE      = alpha

all: $(EXE)

$(EXE): $(C_FILE)
	$(CC) $(CFLAGS) -o $(EXE) $(C_FILE)

$(C_FILE): $(LEX_FILE)
	$(FLEX) $(LEX_FILE)

clean:
	rm -f $(EXE) $(C_FILE)
