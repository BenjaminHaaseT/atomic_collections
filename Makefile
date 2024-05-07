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
TESTBINFILES=$(patsubst $(TESTSRC)/%.c, $(TESTBIN)/%, $(TESTSRCFILES))

build: $(OBJFILES)

build_test: $(TESTBINFILES)

$(TESTBIN)/%_test:$(TESTSRC)/%_test.c $(OBJ)/%.o
	$(CC) -o $@ $^ -I$(INCDIR) -Wall -Werror 

$(OBJ)/%.o:$(SRC)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJFILES) $(DEPFILES) $(TESTBINFILES)

-include $(DEPFILES)

