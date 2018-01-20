P :=
#CC := $(P)arm-xilinx-linux-gnueabi-gcc
CC := $(P)arm-linux-gnueabihf-gcc

CFLAGS := -O2 -Wall -Wextra -pedantic
LIBS := -lm -lpthread
OBJS := log.o hw.o rxla.o
EXENAME := rxla
DEPS := $(OBJS:.o=.d)

$(EXENAME): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $<

clean:
	rm -f $(OBJS) $(EXENAME) $(DEPS)

-include $(DEPS)
