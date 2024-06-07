CC = cc

BINDIR = bin
BINARY = hush

UNAME_S := $(shell uname -s)

CFLAGS = -O3 $(shell mysql_config --cflags) -Dapp_name=$(BINARY) -Dgit_sha=$(shell git rev-parse HEAD)
LDFLAGS = $(shell mysql_config --libs) -lulfius -ljansson -lsodium -lpthread -lorcania

$(BINDIR)/$(BINARY): $(BINDIR) clean
	$(CC) -o $@ main.c base64.c logger.c database.c api.c pass.c $(CFLAGS) $(LDFLAGS)
ifeq ($(UNAME_S),Darwin)
	install_name_tool -change @rpath/libulfius.2.6.dylib /usr/local/lib/libulfius.2.6.3.dylib $@
endif	

$(BINDIR):
	mkdir $@

.PHONY: client
client: $(BINDIR)
	$(CC) -o $(BINDIR)/$@ clients/main.c pass.c -O3 -Dapp_name=$@ -Dgit_sha=$(shell git rev-parse HEAD) -lsodium -lcurl -ljansson

.PHONY: all
all: $(BINDIR) $(BINDIR)/$(BINARY) client clean

.PHONY: clean
clean:
	rm -rf bin/*
