SRC=src
OBJ=obj
INCDIR=include
TESTSRC=test/src
TESTBIN=test/bin

CC=gcc
OPT=-O0
DEPFLAGS=-MP -MD
CFLAGS=-Wall -Werror -g $(foreach D, $(INCDIR), -I$(D)) $(OPT) $(DEPFLAGS)

SRCFILES=$(foreach D, $(SRC), $(wildcard $(D)/*.c))

OBJFILES=$(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SRCFILES))
DEPFILES=$(patsubst $(SRC)/%.c, $(OBJ)/%.d, $(SRCFILES))
TESTSRCFILES=$(foreach D, $(TESTSRC), $(wildcard $(D)/*.c))

build: $(OBJFILES)

$(OBJ)/%.o:$(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJFILES) $(DEPFILES)

-include $(DEPFILES)

