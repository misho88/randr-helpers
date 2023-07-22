CFLAGS=-Wall -Wextra
BIN=/usr/local/bin

all: randr-list randr-watch randr-select

randr-list: randr-list.c
	$(CC) $(CFLAGS) -lxcb -lxcb-randr $< -o $@

randr-watch: randr-watch.c
	$(CC) $(CFLAGS) -lxcb -lxcb-randr $< -o $@

randr-select: randr-select.c
	$(CC) $(CFLAGS) $< -o $@

install: all
	install -d $(DESTDIR)$(BIN)
	install randr-list randr-watch randr-select $(DESTDIR)$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BIN)/randr-list $(DESTDIR)$(BIN)/randr-watch $(DESTDIR)$(BIN)/randr-select
