CFLAGS= -O0 -g $(INCLUDE_DIRS) $(CDEFS)

PRODUCT= aesdsocket

CFILES= aesdsocket.c

SRCS= ${CFILES}
OBJS= ${CFILES:.c=.o}

all:	${PRODUCT}

clean:
	-rm -f *.o *.NEW *~ *.d
	-rm -f ${PRODUCT} ${GARBAGE}

inet_server:	aesdsocket.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ aesdsocket.o $(LIBS)

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
