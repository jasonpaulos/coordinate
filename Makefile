IDIR = include
SDIR = src
ODIR = bin

CC = gcc
CFLAGS = -Wall -Werror -I$(IDIR)

DEPS := $(wildcard $(IDIR)/*.h)
OBJS := $(patsubst $(SDIR)/%.c,$(ODIR)/%.o,$(wildcard $(SDIR)/*.c))

$(ODIR)/%.o: $(SDIR)/%.c $(DEPS)
	@mkdir -p $(@D)
	$(CC) -g -c -o $@ $< $(CFLAGS)

$(ODIR)/coordinate: $(OBJS)
	$(CC) -g -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -rf $(ODIR)
