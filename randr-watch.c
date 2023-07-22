#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <unistd.h>

void
spawn(char ** argv)
{
	pid_t pid = fork();
	if (pid < 0) perror("randr-watch"), exit(1);
	if (pid != 0) return;
	execvp(argv[0], argv);
	perror("randr-watch"), exit(1);
}

int
main(int argc, char ** argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: randr-watch cmd [args...]\n");
		exit(1);
	}
	xcb_connection_t * conn = xcb_connect(NULL, NULL);

	xcb_screen_t * screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	xcb_window_t window = screen->root;
	xcb_randr_select_input(conn, window, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
	xcb_flush(conn);

	xcb_timestamp_t last_time;
	for (xcb_generic_event_t * event; (event = xcb_wait_for_event(conn)); free(event)) {
		if (!(event->response_type & XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE))
			continue;
		xcb_randr_screen_change_notify_event_t * randr_event = (void *)event;
		if (last_time == randr_event->timestamp)
			continue;
		last_time = randr_event->timestamp;
		spawn(argv + 1);
	}
	xcb_disconnect(conn);
	return 0;
}
