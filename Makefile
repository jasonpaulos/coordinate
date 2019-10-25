IDIR = include
SDIR = src
ODIR = bin

CC = gcc
CFLAGS = -Wall -Werror -I$(IDIR)

DEPS = $(IDIR)/coordinate.h
OBJS = $(ODIR)/main.o

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -g -c -o $@ $< $(CFLAGS)

$(ODIR)/coordinate: $(OBJS)
	$(CC) -g -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -rf $(ODIR)
