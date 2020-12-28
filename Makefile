TARGET = sicpy
CC=gcc
OBJS = \
  lex.yy.o\
  y.tab.o\
  main.o\
  interface.o\
  create.o\
  execute.o\
  eval.o\
  string_pool.o\
  util.o\
  native.o\
  error.o\
  memory.o\
  debug.o
CFLAGS = -c -g -Wall -Wswitch-enum -ansi -pedantic
INCLUDES = \

$(TARGET):$(OBJS)
	$(CC) $(OBJS) -o $@ -lm
clean:
	del *.o *.output lex.yy.c y.tab.c y.tab.h *~
y.tab.h : sicpy.y
	bison --yacc -dv sicpy.y
y.tab.c : sicpy.y
	bison --yacc -dv sicpy.y
lex.yy.c : sicpy.l sicpy.y y.tab.h
	flex sicpy.l
y.tab.o: y.tab.c sicpy.h MEM.h
	$(CC) -c -g $*.c $(INCLUDES)
lex.yy.o: lex.yy.c sicpy.h MEM.h
	$(CC) -c -g $*.c $(INCLUDES)
.c.o:
	$(CC) $(CFLAGS) $*.c $(INCLUDES)

############################################################
create.o: create.c MEM.h DBG.h sicpy.h SCP.h
error.o: error.c MEM.h sicpy.h SCP.h
eval.o: eval.c MEM.h DBG.h sicpy.h SCP.h
execute.o: execute.c MEM.h DBG.h sicpy.h SCP.h
interface.o: interface.c MEM.h DBG.h sicpy.h SCP.h
main.o: main.c SCP.h MEM.h
native.o: native.c MEM.h DBG.h sicpy.h SCP.h
string_pool.o: string_pool.c MEM.h DBG.h sicpy.h SCP.h
util.o: util.c MEM.h DBG.h sicpy.h SCP.h
debug.o: debug.c MEM.h DBG.h
memory.o: memory.c MEM.h