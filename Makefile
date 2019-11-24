IDIR = include
SDIR = src
ODIR = bin
EXDIR = examples

CC = gcc
CFLAGS = -fPIC -Wall -Werror -O2 -g -I$(IDIR) -lrt -lpthread -D DEBUG=1
LDFLAGS = -shared

DEPS := $(wildcard $(IDIR)/*.h)
OBJS := $(patsubst $(SDIR)/%.c,$(ODIR)/%.o,$(filter-out $(SDIR)/main.c, $(wildcard $(SDIR)/*.c)))
EXAMPLES := $(wildcard $(EXDIR)/*)

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/libcoordinate.so: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(ODIR)/coordinate: $(ODIR)/main.o
	$(CC) -o $@ $^ $(CFLAGS)

$(EXDIR)/%: $(ODIR)/libcoordinate.so FORCE
	$(MAKE) -C $@

$(EXDIR): $(EXAMPLES) FORCE

.PHONY: clean gdb FORCE default

.DEFAULT_GOAL := default

default: $(ODIR)/coordinate $(EXDIR)

FORCE:

gdb: export LD_LIBRARY_PATH := $(abspath $(ODIR)):$(LD_LIBRARY_PATH)
gdb: $(ODIR)/coordinate
	gdbserver :26100 $(ODIR)/coordinate $(ARGS)

clean:
	rm -rf $(ODIR)
	$(foreach dir,$(EXAMPLES),$(MAKE) -C $(dir) clean)
