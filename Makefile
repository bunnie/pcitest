
CC=gcc
#CC=powerpc-linux-gcc
CFLAGS=-Wall -g

main: pcimem pcitest

clean:
	-rm -f *.o *~ core pcimem pcitest

