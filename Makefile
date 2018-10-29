# ff-sort
# change this...
CC ?= cc
CFLAGS ?= -Os
CPPFLAGS += -pedantic -Wall -Wextra
FARBHERD=1

DESTDIR ?= /usr/local

# but not this..
CPPFLAGS += -DFARBHERD=$(FARBHERD)

BINS=ff-sort
all: $(BINS)

ff-sort: ff-sort.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: $(BINS)
	install $(BINS) $(DESTDIR)/bin

clean:
	rm -f $(BINS)
