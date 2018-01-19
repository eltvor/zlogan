#CC=arm-xilinx-linux-gnueabi-gcc

INC_DIR=

CFLAGS=-O -Wall -ggdb
LIBS=-lm -lpthread
OBJS=log.o hw.o rxla.o
EXENAME=rxla

$(EXENAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIB_DIR) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INC_DIR) -c $<

clean:
	rm -f $(OBJS) $(EXENAME)
