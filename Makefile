IDIR = include
SDIR = src
ODIR = bin

CC = gcc
CFLAGS = -Wall -Werror -I$(IDIR) -lpthread -D DEBUG=1 -Wno-unused-command-line-argument -lm

DEPS := $(wildcard $(IDIR)/*.h)
OBJS := $(patsubst $(SDIR)/%.c,$(ODIR)/%.o,$(wildcard $(SDIR)/*.c))

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -g -c -o $@ $< $(CFLAGS)

$(ODIR)/coordinate: $(OBJS)
	$(CC) -g -o $@ $^ $(CFLAGS)

.PHONY: clean gdb

gdb: $(ODIR)/coordinate
	gdbserver :26100 $(ODIR)/coordinate $(ARGS)

clean:
	rm -rf $(ODIR)
