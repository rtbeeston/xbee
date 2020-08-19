TARGETS=xbee

all : $(TARGETS)

xbee	: xbee.o sp.o api.o getch.o
	cc -o xbee xbee.o sp.o api.o getch.o -lpthread

xbee.o	: xbee.c

sp.o	: sp.c sp.h

api.o	: api.c api.h

getch.o : getch.c getch.h

sp_send_receive : sp_send_receive.o getch.o

clean :
	rm -f *.o $(TARGETS)
