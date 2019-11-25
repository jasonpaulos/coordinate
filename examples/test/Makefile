NAME = test
COORDINATE_DIR = $(abspath ../..)
IDIR = .
SDIR = .
ODIR = bin

CC = gcc
CFLAGS = -Wall -Werror -I$(COORDINATE_DIR)/include -L$(COORDINATE_DIR)/bin/loc -lcoordinate -lpthread -D DEBUG=1
COORDINATE_LIB = $(COORDINATE_DIR)/bin/loc/libcoordinate.so

DEPS := $(wildcard $(IDIR)/*.h)
OBJS := $(patsubst $(SDIR)/%.c,$(ODIR)/%.o,$(wildcard $(SDIR)/*.c))

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS) $(COORDINATE_LIB)
	@mkdir -p $(@D)
	$(CC) -g -c -o $@ $< $(CFLAGS)

$(ODIR)/$(NAME): $(OBJS) $(COORDINATE_LIB)
	$(CC) -g -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -rf $(ODIR)
