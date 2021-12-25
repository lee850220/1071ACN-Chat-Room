 # ---------------------------------------------INFORMATION--------------------------------------------
 # 	Project Name: Advanced Computer Networking HW3
 #	Author: Kelvin Lee 李冠霖
 #	Version: 1.7 (stable)
 #	Environment: Linux
 # 	Date: 2018/10/16  10:43
 # ===================================================================================================*/
SHELL := /bin/bash
CFLAGS := -g -Wall -pthread
CC = gcc
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
EXE = $(OBJ:.o=)

.PHONY: clean dep main help changelog

all: clean dep main

dep:
	@touch .depend
	@echo creating dependency file...
	@for n in $(SRC); do \
		$(CC) $(CFLAGS) -E -MM $$n >> .depend; \
	done
-include .depend

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

main: $(OBJ)
	@echo
	@for i in $(EXE); do \
		echo \ \ \ [CC] \ \ \ \  $${i}; \
		$(CC) $(CFLAGS) $${i}.o -o $${i}; \
	done
	@echo

debugging: CFLAGS += -D __DEBUGGING__
debugging: all

help:
	@echo
	@echo '####--------------------------- Manual ---------------------------####'
	@echo
	@head -5 client.c > tmp
	@sed -i '1d' tmp
	@cat tmp | tr '*' ' '
	@head -3 server.c > tmp
	@sed -i '1d' tmp
	@cat tmp | tr '*' ' '
	@rm tmp
	@echo

changelog:
	@echo
	@echo '####--------------------------- ChangeLog ---------------------------####'
	@echo
	@cat ChangeLog.txt
	@echo

clean:
	@rm -fv $(OBJ) $(EXE) .depend

#============================================= Optional =============================================
netstat:
	@netstat -tulpn | grep LISTEN

test: $(TEST:.c=.o)
	$(CC) $(CFLAGS) $? -o $@.exe

debug:
	valgrind --leak-check=full ./$(DIR)/$(TEST) > /dev/null 2> res2
	valgrind -v --track-origins=yes --leak-check=full ./$(DIR)/$(TEST) > /dev/null 2> res3
