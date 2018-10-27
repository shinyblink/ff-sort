CC ?= cc
CFLAGS ?= -Os
CPPFLAGS += -pedantic -Wall -Wextra

DESTDIR ?= /usr/local

BINS=ff-sort
all: $(BINS)

ff-sort: ff-sort.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: $(BINS)
	install $(BINS) $(DESTDIR)/bin

clean:
	rm -f $(BINS)
