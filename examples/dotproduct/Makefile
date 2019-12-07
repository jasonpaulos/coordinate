NAME = dotproduct
COORDINATE_DIR = $(abspath ../..)
IDIR = .
SDIR = .
ODIR = bin

CC = gcc
CFLAGS_DEBUG = -g -D DEBUG=1
# CFLAGS_DEBUG = -O3 -D DEBUG=0
CFLAGS = $(CFLAGS_DEBUG) -Wall -Werror -I$(COORDINATE_DIR)/include -L$(COORDINATE_DIR)/bin/loc -lrt -lcoordinate -lpthread
COORDINATE_LIB = $(COORDINATE_DIR)/bin/loc/libcoordinate.so

DEPS := $(wildcard $(IDIR)/*.h)
OBJS := $(patsubst $(SDIR)/%.c,$(ODIR)/%.o,$(wildcard $(SDIR)/*.c))

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS) $(COORDINATE_LIB)
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/$(NAME): $(OBJS) $(COORDINATE_LIB)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -rf $(ODIR)
