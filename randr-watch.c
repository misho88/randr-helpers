#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <unistd.h>
#include <sys/wait.h>

void
spawn(char ** argv)
{
	pid_t pid = fork();
	if (pid < 0) perror("randr-watch"), exit(1);
	if (pid != 0) {
		if (waitpid(pid, NULL, 0) < 0) perror("randr-watch"), exit(1);
		return;
	};
	execvp(argv[0], argv);
	perror("randr-watch"), exit(1);
}

void
usage()
{
	fprintf(stderr, "usage: randr-watch cmd [-n|--now] [args...]\n");
	exit(1);

}

int
main(int argc, char ** argv)
{
	if (argc < 2 || argv[1][0] == '\0') usage();
	++argv;
	if (argv[0][0] == '-') {
		if (strcmp(argv[0], "-n") == 0 || strcmp(argv[0], "--now") == 0) {
			spawn(++argv);
		}
		else {
			usage();
		}
	}
	xcb_connection_t * conn = xcb_connect(NULL, NULL);

	xcb_screen_t * screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	xcb_window_t window = screen->root;
	xcb_randr_select_input(conn, window, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
	xcb_flush(conn);

	for (xcb_generic_event_t * event; (event = xcb_wait_for_event(conn)); free(event))
		if (event->response_type & XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE)
			spawn(argv);
	xcb_disconnect(conn);
	return 0;
}
