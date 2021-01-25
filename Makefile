# Makefile for KnightCap

# what C compiler? It better be Ansi-C. Use gcc if you have it.
# you may find that KnightCap is very slow if you don't use gcc
CC = gcc

# What compiler switches do you want? These ones work well with gcc
OPT = -DXX__INSURE__ -O3 -fshort-enums -fsigned-char -Wall -DTRIDGE=1 -DUSE_SHM=1 -DUSE_SMP=0 -DDEBUG_FRIENDLY=0
# OPT = -fsigned-char -g -fshort-enums -Wall -DTRIDGE=1 -DUSE_SHM=1 -DUSE_SMP=0 -DDEBUG_FRIENDLY=1

# If you don't have gcc then perhaps try this instead. You only need
# the null definition for inline if your C compiler doesn't know about
# the inline keyword
# OPT = -O -Dinline=""

# if you want profiling (for performance tuning) then uncomment
# the following (some systems may not support this properly)
LOPT = -lm


# comment out the following 7 lines if you don't want the rendered 
# display (or you don't have OpenGL and Glut libraries)
# If you do have these libs then make sure the first three
# lines point at the right places
#MESA = /usr/local/src/Mesa-3.0
#GLUT = $(MESA)
#X11 = /usr/X11R6
GLUT_LIBS = -lglut
MESA_LIBS = -lGLU -lGL -lm
XLIBS = -L$(X11)/lib -lXmu -lXt -lXext -lX11 -lXi
DISPLAYFLAGS = -DRENDERED_DISPLAY=1


# you shouldn't need to edit anything below this line. Unless
# something goes wrong.

INCLUDE = $(DISPLAYFLAGS)
CFLAGS = $(OPT) $(INCLUDE) 

LIBS = $(GLUT_LIBS) $(MESA_LIBS) $(XLIBS) 

TARGET = KnightCap

OBJS2 = tactics.o util.o move.o eval.o 
OBJS = knightcap.o trackball.o board.o generate.o movement.o \
	ordering.o hash.o log.o prog.o timer.o ics.o display.o \
	testsuite.o brain.o td.o book.o shmem.o parse.o dump.o \
	tune.o $(OBJS2)

$(TARGET):  $(OBJS) 
	-mv $@ $@.old
	$(CC) $(LOPT) -o $@ $(OBJS) $(LIBS)
	sum KnightCap

proto:
	cat *.c | awk -f mkproto.awk > proto.h

clean:
	/bin/rm -f *.o *~ $(TARGET)

