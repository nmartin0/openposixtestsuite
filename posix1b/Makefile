SOURCES = posix_mman.c posix_time.c semaphore.c mqueue.c 
SO_VERSION=1.09
OBJECTS = $(SOURCES:.c=.o)

LIB_STATIC = libposix1b.a
LIB_DYNAMIC = libposix1b.so.$(SO_VERSION)
LIB_INSTALL_PATH=/usr/lib

CC=gcc
CFLAGS=-shared -Wall -fpic -fPIC -O0 -g -c -I. 

#
# If your application is multithreaded add option -D_REENTRANT to CCFLAGS
#
AR = ar
ARFLAGS = -cru

LIBS = $(LIB_STATIC) $(LIB_DYNAMIC)($(OBJECTS))
TESTS=testimer testmsg testsem

all: $(LIBS) $(TESTS)

libs: $(LIBS)

$(LIB_STATIC): posix_mman.o posix_time.o semaphore.o mqueue.o
	$(AR) $(ARFLAGS) $(LIB_STATIC) posix_mman.o posix_time.o semaphore.o mqueue.o

lib%.so.$(SO_VERSION)($(OBJECTS)): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$@ -o $@ $(OBJECTS)

testimer: testimer.o $(LIB_DYNAMIC)
	$(CC) -o testimer testimer.c -L. $(LIB_DYNAMIC)

testmsg: testmsg.o $(LIB_DYNAMIC)
	$(CC) -o testmsg testmsg.c -L. $(LIB_DYNAMIC)

testsem: testsem.o $(LIB_DYNAMIC)
	$(CC) -o testsem testsem.c -L. $(LIB_DYNAMIC)


clean: 
	rm -f *.a *~ *.o *.so core $(LIB_DYNAMIC) $(LIB_STATIC) $(TESTS)

tgz:	clean
	cd ..; tar cvzf posix1b.tgz posix1b

copytgz: tgz
	mcopy -o ../posix1b.tgz a:

install: $(LIBS)
	cp $(LIB_DYNAMIC) $(LIB_INSTALL_PATH)
	cp $(LIB_STATIC) $(LIB_INSTALL_PATH)

