#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void
die(const char * msg)
{
	perror(msg);
	exit(1);
}

struct pipe { int rfd, wfd; }
mk_pipe()
{
	int fds[2];
	if (pipe(fds) < 0) die("pipe() failed");
	return (struct pipe){ .rfd = fds[0], .wfd = fds[1] };
}

void
move(int old, int new)
{
	if (dup2(old, new) < 0) die("dup2() failed");
}

void
await(pid_t pid)
{
	int status;
	if (waitpid(pid, &status, 0) < 0) die("waitpid() failed");
}

void
exec(char ** argv)
{
	for (int i = 0; argv[i] != NULL; i++) {
		if (argv[i][0] == '\0') {
			argv[i] = NULL;
			break;
		}
	}
	execvp(argv[0], argv);
	die("execvp() failed");
}

void
add_digit(int * i, char d)
{
	if (d < '0' || d > '9') die("expected digit");
	*i *= 10;
	*i += d - '0';
}

struct output { char name[32], manufacturer[4]; int model, serial; struct output * next; };

struct output_parse_state { struct output * output; int i; };

typedef void * (*output_parse_func)(struct output_parse_state * s, char c);

output_parse_func output_parse_name(struct output_parse_state * s, char c);
output_parse_func output_parse_manufacturer(struct output_parse_state * s, char c);
output_parse_func output_parse_model(struct output_parse_state * s, char c);
output_parse_func output_parse_serial(struct output_parse_state * s, char c);

output_parse_func
output_parse_next_output(struct output_parse_state * s)
{
	s->output->next = calloc(1, sizeof(struct output));
	if (s->output->next == NULL) die("calloc() failed");
	s->output = s->output->next;
	s->i = 0;
	return (output_parse_func)output_parse_name;
}

output_parse_func
output_parse_reset_i(struct output_parse_state * s, void * next)
{
	s->i = 0;
	return (output_parse_func)next;
}

output_parse_func
output_parse_name(struct output_parse_state * s, char c)
{
	if (c == EOF) return NULL;
	if (c == ':') return output_parse_reset_i(s, output_parse_manufacturer);
	if (c == ',') return output_parse_next_output(s);
	if (s->i >= 31) die("name longer than 31 characters");
	s->output->name[s->i++] = c;
	return (output_parse_func)output_parse_name;
}

output_parse_func
output_parse_manufacturer(struct output_parse_state * s, char c)
{
	if (c == EOF) return NULL;
	if (c == ':') return output_parse_reset_i(s, output_parse_model);
	if (c == ',') return output_parse_next_output(s);
	if (s->i >= 3) die("manufacturer longer than 3 characters");
	s->output->manufacturer[s->i++] = c;
	return (output_parse_func)output_parse_manufacturer;
}

output_parse_func
output_parse_model(struct output_parse_state * s, char c)
{
	if (c == EOF) return NULL;
	if (c == ':') return (output_parse_func)output_parse_serial;
	if (c == ',') return output_parse_next_output(s);
	add_digit(&s->output->model, c);
	return (output_parse_func)output_parse_model;
}

output_parse_func
output_parse_serial(struct output_parse_state * s, char c)
{
	if (c == EOF) return NULL;
	if (c == ':') die("too many colons (:)");
	if (c == ',') return output_parse_next_output(s);
	add_digit(&s->output->serial, c);
	return (output_parse_func)output_parse_serial;
}

struct output *
get_outputs()
{
	struct pipe pipe = mk_pipe();
	pid_t pid = fork();

	if (pid == 0) {
		close(pipe.rfd);
		move(pipe.wfd, STDOUT_FILENO);
		exec((char * []){ "randr-list", NULL });
	}

	close(pipe.wfd);

	FILE * stream = fdopen(pipe.rfd, "r");
	if (stream == NULL) die("fdopen() failed");

	struct output * head = calloc(1, sizeof(struct output));
	if (head == NULL) die("calloc() failed");

	struct output_parse_state state = { head, 0 };
	output_parse_func f = (output_parse_func)output_parse_name;
	for (char c = fgetc(stream); f != NULL && c != EOF; c = fgetc(stream)) {
		f = f(&state, c);
	}

	fclose(stream);
	await(pid);
	return head;
}

struct output *
get_arg_outputs(char * arg)
{	struct output * head = calloc(1, sizeof(struct output));
	if (head == NULL) die("calloc() failed");

	struct output_parse_state state = { head, 0 };
	output_parse_func f = (output_parse_func)output_parse_name;
	for (int i = 0; f != NULL && arg[i] != '\0'; i++) {
		f = f(&state, arg[i]);
	}
	return head;
}

void
free_outputs(struct output * output)
{
	if (output == NULL) return;
	free_outputs(output->next);
	free(output);
}

bool
strings_match(char * a, char * b)
{
	return strcmp(a, b) == 0;
}

bool
ints_match(int a, int b)
{
	return a == 0 || b == 0 || a == b;
}

bool
outputs_match(struct output * a, struct output * b)
{
	if (a == NULL || b == NULL)
		return a == b;
	if (!strings_match(a->name, b->name))
		return false;
	if (!strings_match(a->manufacturer, b->manufacturer))
		return false;
	if (!ints_match(a->model, b->model))
		return false;
	if (!ints_match(a->serial, b->serial))
		return false;
	return true;
}

bool
match(const char * arg, const char * lopt)
{
	if (arg[0] != '-') return false;
	if (arg[1] == lopt[0]) return arg[2] == '\0';
	if (arg[1] != '-') return false;
	return strcmp(arg + 2, lopt) == 0;
}

void
usage()
{
	fprintf(
		stderr,
		"usage: randr-select CASE CMD ...\n"
		"CMD: ''-terminated argv\n"
		"CASE: --case|-c COND --then|-t  OR  --default|-d\n"
		"COND: output specification OR --and|-a OR --or|-o\n"
	);
	exit(1);
}

int main(int argc, char ** argv)
{
	if (argc < 2) usage();
	struct output * outputs = get_outputs();

	int i = 1;
	bool cond = false, match_found;
	struct output * arg_output;

	enum { DONE = 0, BAD, CD, AOT, AND, OR, EXEC, SKIP, RAN_OUT } state = CD;
	while (state) switch(state) {
	case CD:
		if (i >= argc) { state = RAN_OUT; break; };
		if      (match(argv[i], "case"   )) cond = true, i++, state = AND;
		else if (match(argv[i], "default")) i++, state = EXEC;
		else                                state = BAD;
		break;
	case AOT:
		if (i >= argc) {
			fprintf(stderr, "need -a|--and, -o|--or or -t|--then\n");
			return 1;
		};
		if      (match(argv[i], "and"    )) cond = true, i++, state = AND;
		else if (match(argv[i], "or"     )) i++, state = OR;
		else if (match(argv[i], "then"   )) i++, state = cond ? EXEC : SKIP;
		else                                state = BAD;
		break;
	case AND:
	case OR:
		if (i >= argc) { state = RAN_OUT; break; };
		if (argv[i][0] == '-') { state = BAD; break; }
		if ((state == OR) ^ (cond == false)) { i++; state = AOT; break; }

		arg_output = get_arg_outputs(argv[i]);
		if (arg_output->next) {
			fprintf(stderr, "cannot chain outputs with ',' (use -a|--and or -r|--or): %s\n", argv[i]);
			exit(1);
		}
		match_found = false;
		for (struct output * o = outputs; o && !match_found; o = o->next)
			match_found = outputs_match(o, arg_output);
		free(arg_output);

		cond = state == AND ? cond & match_found : cond | match_found;
		i++;
		state = AOT;
		break;
	case SKIP:
		if (i >= argc) { state = RAN_OUT; break; };
		state = argv[i++][0] == '\0' ? CD : SKIP;
		break;
	case EXEC:
		free_outputs(outputs);
		exec(argv + i);
		break;
	case BAD:
		fprintf(stderr, "unexpected: %s\n", argv[i]);
		usage();
		break;
	case RAN_OUT:
		fprintf(stderr, "ran out of arguments without a match\n");
		return 1;
	default:
		fprintf(stderr, "broken FSM\n");
		exit(1);
		break;

	}

	free_outputs(outputs);
	return 0;
}
