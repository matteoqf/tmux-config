.PHONY: all install clean

CC = clang
CFLAGS = -O2 -Wall

all: bin/tmux_left_bin bin/tmux_status_daemon bin/tmux_status_read

bin/tmux_left_bin: src/tmux_left.c
	$(CC) $(CFLAGS) -o $@ src/tmux_left.c -lm

bin/tmux_status_daemon: src/tmux_status_daemon.c
	$(CC) $(CFLAGS) -o $@ src/tmux_status_daemon.c -lm

bin/tmux_status_read: src/tmux_status_read.c
	$(CC) $(CFLAGS) -o $@ src/tmux_status_read.c

install: all
	install -d $(DESTDIR)$(HOME)/.local/bin
	install -m755 bin/* scripts/* $(DESTDIR)$(HOME)/.local/bin/
	install -m644 .tmux.conf $(DESTDIR)$(HOME)/.tmux.conf

clean:
	rm -f bin/tmux_left_bin bin/tmux_status_daemon bin/tmux_status_read
