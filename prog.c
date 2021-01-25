#include "includes.h"
#include "knightcap.h"

#ifdef SUNOS4
#define memmove memcpy
#endif

static int fd_in = -1;
static int fd_out = -1;
static FILE *f_out;

static int child_pid;


extern struct state *state;

void prog_printf(char *format_str, ...)
{
	va_list ap;  

	if (!f_out) return;

	va_start(ap, format_str);
	vfprintf(f_out,format_str,ap);
	va_end(ap);

#if 0
	fprintf(stdout, "[");
	va_start(ap, format_str);
	vfprintf(stdout,format_str,ap);
	va_end(ap);
#endif
}



void prog_start(char *prog)
{
	int fd1[2], fd2[2];

	if (pipe(fd1) || pipe(fd2)) {
		perror("pipe");
		return;
	}

	child_pid = fork();

	if (child_pid) {
		fd_in = fd1[0];
		fd_out = fd2[1];
		close(fd1[1]);
		close(fd2[0]);
		f_out = fdopen(fd_out, "w");
		setlinebuf(f_out);
		return;
	}

	close(0);
	close(1);

	if (dup(fd2[0]) != 0 ||
	    dup(fd1[1]) != 1) {
		fprintf(stderr,"Failed to setup pipes\n");
		exit(1);
	}

	close(fd1[0]);
	close(fd2[1]);
	close(fd1[0]);
	close(fd2[1]);

	exit(system(prog));
}

int prog_running(void)
{
	if (!process_exists(child_pid)) return 0;
	return f_out != NULL;
}

void prog_tell_move(Position *p, Move *move)
{
	prog_printf("%s\n",short_movestr(p, move));
}

void prog_script(const char *script)
{
	FILE *f = fopen(script,"r");
	char line[200];

	if (!f) return;

	while (fgets(line, sizeof(line)-1, f)) {
		if (line[strlen(line)-1] == '\n') {
			line[strlen(line)-1] = 0;
		}
		if (line[0] == '&') {
			prog_script(&line[1]);
		} else {
			prog_printf("%s\n", line);
		}
	}
	fclose(f);
}

static int parse_prog_move(char *line,Move *move,int player)
{
	int ret, move_num;
	char movebuf[100];
	Piece promotion;

	if ((ret = sscanf(line,"%d. ... %s",&move_num,movebuf)) >= 2 &&
	    parse_move(movebuf, &state->position, move)) {
		return 1;
	}

	if (parse_ics_move(line, player, move, &promotion)) {
		return 1;
	}

	return 0;
}


int prog_check_move(Move *move, int player)
{
	static char line[1000];
	static int line_len, print_len;
	fd_set set;
	struct timeval tval;
	int n, found=0;
	char *p;

	if (fd_in == -1)
		return 0;

	FD_ZERO(&set);
	FD_SET(fd_in, &set);

	tval.tv_sec = 0;
	tval.tv_usec = 0;

	while (!found) {		
		if (select(fd_in+1, &set, NULL, NULL, &tval) != 1)
			break;

		n = read(fd_in, line+line_len, sizeof(line) - (line_len+1));
		if (n <= 0) break;

		line[line_len+n] = 0;
		
		while ((p=strchr(line,'\n'))) {
			*p++ = 0;
			if (parse_prog_move(line, move, player)) {
				found = 1;
			}
			if (strstr(line,"<12>") == NULL)
				lprintf(0,"%s\n", line+print_len);
			memmove(line, p, sizeof(line)-(p-line));
			print_len = 0;
		}

		line_len = strlen(line);
		if (line_len > 0 && strstr(line,"<12>") == NULL) {
			lprintf(0,"%s", line);
			print_len = line_len;
		}
	}

	return found;
}

void prog_exit(void)
{
	if (f_out) {
		prog_printf("quit\n");
		close(fd_in);
	}

	if (child_pid) {
		kill(child_pid, SIGINT);
		kill(child_pid, SIGINT);
		kill(child_pid, SIGTERM);
	}
}
