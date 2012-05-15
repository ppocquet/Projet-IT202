.PHONY : default clean clean-all

CC=gcc
CFLAGS = -DORDO_PRIO -W -Wall -g `pkg-config --cflags glib-2.0`
LDFLAGS = `pkg-config --libs glib-2.0` -lpthread


VPATH=./:application_parallele/:test/
SRC=$(wildcard $(patsubst %/,%/*.c,$(subst :, ,$(VPATH))))
DEP=$(patsubst %.c, %.d, $(SRC))
OBJ=$(patsubst %.c, %.o, $(SRC))

TARGET=example fibonacci sum_table test_ordo

BOLD="\033[01m"
NORMAL="\033[0m"
RED="\033[01m\033[31m"
GREEN="\033[01m\033[32m"
YELLOW="\033[01m\033[33m"
OK=echo $(GREEN)"OK"$(NORMAL)
FAILED=echo $(RED)"FAILED"$(NORMAL)
ERROR=if [ -s .tmp ]; then echo -n $(YELLOW); cat .tmp | sed 's|^|\t|g'; echo $(NORMAL); fi


default : example

clean :
	@echo $(BOLD)"Nettoyage..."$(NORMAL)
	@rm -vf $(OBJ) $(DEP) *~ | sed 's|^|\t|g'
	@rm -f .tmp


clean-all :
	@echo $(BOLD)"Nettoyage..."$(NORMAL)
	@rm -vf $(OBJ) *~ $(DEP) $(TARGET) | sed 's|^|\t|g'
	@rm -f .tmp

all : $(TARGET)

fibonacci : thread.o fibonacci.o

test_preempt : test_preempt.o thread.o

test_lib : test_lib.o
	@echo -n $(BOLD)"$(CC) -o $@ $^ $(LDFLAGS) : "$(NORMAL)
	@($(CC) -o $@ $^ -lpthread 2> .tmp && $(OK) && $(ERROR)) || ($(FAILED) && $(ERROR) && false)

sum_table : thread.o sum_table.o

test_signal : thread.o test_signal.o

test_stop : thread.o test_stop.o

example : example.o thread.o

test_ordo : test_ordo.o thread.o

% : %.o
	@echo -n $(BOLD)"$(CC) -o $@ $^ $(LDFLAGS) : "$(NORMAL)
	@($(CC) -o $@ $^ $(LDFLAGS) 2> .tmp && $(OK) && $(ERROR)) || ($(FAILED) && $(ERROR) && false)

%.d : %.c
	@$(CC) $(CFLAGS) -M -o $@ $<

-include $(DEP)

%.o : %.c
	@echo -n $(BOLD)"$(CC)-o $@ -c $< : "$(NORMAL)
	@($(CC) $(CFLAGS) -o $@ -c $< 2> .tmp && $(OK) && $(ERROR)) || ($(FAILED) && $(ERROR) && false)