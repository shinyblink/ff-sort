# ff-sort
# change this...
CC ?= cc
CFLAGS ?= -Os
CPPFLAGS += -pedantic -Wall -Wextra
FARBHERD=0

PREFIX ?= /usr/local
DESTDIR ?= /

# but not this..
CPPFLAGS += -DFARBHERD=$(FARBHERD)

BINS=ff-sort
all: $(BINS)

ff-sort: ff-sort.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm

install: $(BINS)
	install -d $(DESTDIR)/$(PREFIX)/bin
	install $(BINS) $(DESTDIR)/$(PREFIX)/bin

clean:
	rm -f $(BINS)
