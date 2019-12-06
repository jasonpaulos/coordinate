IDIR = include
EXDIR = examples

SDIR = src
SDIR_CLI = $(SDIR)/cli

ODIR = bin
ODIR_CLI = $(ODIR)/cli
ODIR_LOC = $(ODIR)/loc
ODIR_DSM = $(ODIR)/dsm

INSTALL_DIR = /usr/local

CC = gcc
CFLAGS_CLI = -Wall -Werror -g
CFLAGS_DSM = $(CFLAGS_CLI) -fPIC -I$(IDIR) -lrt -lpthread -D DEBUG=1
CFLAGS_LOC = $(CFLAGS_DSM) -D COORDINATE_LOCAL
LDFLAGS = -shared -Wl,-soname,$(@F)

DEPS := $(wildcard $(IDIR)/*.h)
SRC := $(wildcard $(SDIR)/*.c)
OBJS_CLI := $(patsubst $(SDIR_CLI)/%.c,$(ODIR_CLI)/%.o,$(wildcard $(SDIR_CLI)/*.c))
OBJS_LOC := $(patsubst $(SDIR)/%.c,$(ODIR_LOC)/%.o,$(SRC))
OBJS_DSM := $(patsubst $(SDIR)/%.c,$(ODIR_DSM)/%.o,$(SRC))
EXAMPLES := $(wildcard $(EXDIR)/*)

LIB_LOC = $(ODIR_LOC)/libcoordinate.so
LIB_DSM = $(ODIR_DSM)/libcoordinate.dsm.so
CLI = $(ODIR_CLI)/coordinate

$(ODIR_LOC)/%.o: $(SDIR)/%.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS_LOC)

$(ODIR_DSM)/%.o: $(SDIR)/%.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS_DSM)

$(ODIR_CLI)/%.o: $(SDIR_CLI)/%.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS_CLI)

$(LIB_LOC): $(OBJS_LOC)
	$(CC) -o $@ $^ $(LDFLAGS)

$(LIB_DSM): $(OBJS_DSM)
	$(CC) -o $@ $^ $(LDFLAGS)

$(CLI): $(OBJS_CLI)
	$(CC) -o $@ $^ $(CFLAGS_CLI)

$(EXDIR)/%: $(LIB_LOC) $(LIB_DSM) FORCE
	$(MAKE) -C $@

$(EXDIR): $(EXAMPLES) FORCE

.PHONY: install clean gdb FORCE default

.DEFAULT_GOAL := default

default: $(EXDIR) $(CLI)

FORCE:

gdb: $(CLI)
	gdbserver :26100 $(CLI) $(ARGS)

clean:
	rm -rf $(ODIR)
	$(foreach dir,$(EXAMPLES),$(MAKE) -C $(dir) clean;)

install: $(LIB_LOC) $(LIB_DSM) $(CLI)
	cp $(LIB_LOC) $(INSTALL_DIR)/lib/libcoordinate.so
	cp $(LIB_DSM) $(INSTALL_DIR)/lib/libcoordinate.dsm.so
	ldconfig
	cp $(CLI) $(INSTALL_DIR)/bin/coordinate
