CFLAGS=-Wall -Wextra
BIN=/usr/local/bin
EXE=randr-case randr-current randr-list randr-select randr-watch randr-which-outputs

all: randr-list randr-watch randr-select

randr-list: randr-list.c
	$(CC) $(CFLAGS) -lxcb -lxcb-randr $< -o $@

randr-watch: randr-watch.c
	$(CC) $(CFLAGS) -lxcb -lxcb-randr $< -o $@

randr-select: randr-select.c
	$(CC) $(CFLAGS) $< -o $@

install: all
	install -d $(DESTDIR)$(BIN)
	install $(EXE) $(DESTDIR)$(BIN)

uninstall:
	cd $(DESTDIR)$(BIN) && rm -f $(EXE)
