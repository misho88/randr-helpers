#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

pid_t
spawn(char ** argv)
{
	pid_t pid = fork();
	if (pid < 0) perror("randr-watch"), exit(1);
	if (pid != 0) {
		return pid;
	};
	execvp(argv[0], argv);
	perror("randr-watch"), exit(1);
}

void
await(pid_t pid, int sig)
{
	if (pid <= 0) {
		return;
	}
	if (sig) {
		if (kill(pid, sig) < 0) perror("randr-watch: kill()"), exit(1);
	}
	if (waitpid(pid, NULL, 0) < 0) perror("randr-watch: waitpid()"), exit(1);
}

pid_t current_pid = 0;

void on_sigterm(int signum)
{
	await(current_pid, SIGTERM);
	exit(0x80 | signum);
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

	struct sigaction sa = { .sa_handler = on_sigterm, .sa_flags = 0 };
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGTERM, &sa, NULL) < 0) perror("randr-watch: sigaction()"), exit(1);

	xcb_connection_t * conn = xcb_connect(NULL, NULL);

	xcb_screen_t * screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
	xcb_window_t window = screen->root;
	xcb_randr_select_input(conn, window, XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE);
	xcb_flush(conn);

	for (xcb_generic_event_t * event; (event = xcb_wait_for_event(conn)); free(event)) {
		if (event->response_type & XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE) {
			await(current_pid, SIGTERM);
			current_pid = spawn(argv);
		}
	}
	await(current_pid, SIGTERM);
	xcb_disconnect(conn);
	return 0;
}
