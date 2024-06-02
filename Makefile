CC = cc

BINDIR = bin
BINARY = hush

UNAME_S := $(shell uname -s)

CFLAGS = -O3 $(shell mysql_config --cflags)
LDFLAGS = $(shell mysql_config --libs) -lulfius -ljansson -lsodium -lpthread

$(BINDIR)/$(BINARY): $(BINDIR) clean
	$(CC) -o $@ main.c logger.c database.c api.c pass.c $(CFLAGS) $(LDFLAGS)
ifeq ($(UNAME_S),Darwin)
	install_name_tool -change @rpath/libulfius.2.6.dylib /usr/local/lib/libulfius.2.6.3.dylib $@
endif	

$(BINDIR):
	mkdir $@

.PHONY: clean
clean:
	rm -rf bin/*
