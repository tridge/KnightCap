/* a 3D chess set

   Andrew.Tridgell@anu.edu.au January 1997 */

#include "includes.h"
#include "trackball.h"
#include "knightcap.h"

int window_size = 500;

#define SLEEP_TIME 100 /* microseconds */
#define REFRESH_TIME 10 /* in seconds */
#define SEEK_WAIT_TIME 120 /* in seconds */
#ifndef MULL_WAIT_TIME
#define MULL_WAIT_TIME 1
#endif

struct state *state;

int demo_mode=0;

int seeking=0;
int seek_answering=0;

int tune_nps=0;
static int opponent;
static int always_think;
static int no_display;
static int ascii_display;
static int use_mulling=1;
static int ics_mode;
static int move_pid;
int display_pid;
static int match_remaining;
static int match_total;
static int def_hash_table_size = DEFAULT_HASH_TABLE_SIZE;
static float match_points;
int color_display=1;
static int auto_exit;
static int def_cpus;

int bad_eval_fd;
char noplay_list[5000];
char opponents_list[5000];

extern int need_redraw;

#if LEARN_EVAL
int learning=1;
#else
int learning=0;
#endif

static float default_move_time = DEFAULT_MOVE_TIME;

char *ics_name = "KnightCap";
char *ics_master = NULL;
char *coeffs_file = NULL;
char *bad_eval_file = "bad_eval.dat";

char play_target[100];

#ifndef SHM_R
#define SHM_R 0400
#endif

#ifndef SHM_W
#define SHM_W 0200
#endif

void sig_wake_up(void)
{
	signal(SIGUSR1, sig_wake_up);
}

void wake_up(void)
{
	kill(display_pid, SIGUSR1);
}

/* setup a standard chess starting position */
void reset_board(void)
{
	setup_board(&state->position);

	state->computer = 0;
	state->moved = 0;
	state->always_think = always_think;
	state->use_mulling = use_mulling;
	state->need_reset = 1;
	if (state->always_think)
		lprintf(0,"Thinking on opponents time\n");
	timer_reset();
	eval_reset();
#if LEARN_EVAL
	state->stored_move_num = 0;
	learning=1;
	state->won = UNKNOWN;
#endif
}

static void display_move(Move *move)
{
	print_move(&state->position, move);
	do_move(&state->position, &state->position, move);
	redraw();
}

int player_moved(Square from, Square to)
{
	Move move;

	move.from = from;
	move.to = to;
	move.v = INFINITY;

	if (!legal_move(&state->position, &move)) {
		lprintf(0, "Illegal move %s\n",
			short_movestr(&state->position, &move));
		return 0;
	}

	state->game_record[state->position.move_num] = move;
	display_move(&move);

	return 1;
}


static void about(void)
{
	lprintf(0,"\nThe author of KnightCap is Andrew Tridgell. He can be contacted\n" \
"via email Andrew.Tridgell@anu.edu.au.\n" \
"\n" \
"The ftp site for KnightCap is ftp://samba.anu.edu.au/pub/KnightCap/\n" \
"\n" \
"KnightCap is free software distributed under the GNU Public License\n" \
"\n" \
);
}


static void help(void)
{
	lprintf(0,"\n" \
"go                              start searching\n" \
"black                           computer plays black\n" \
"white                           computer plays white\n" \
"new                             reset the board\n" \
"time <hsecs>                    update computers time in hundredths of a second\n" \
"otim <hsecs>                    update oppenents time in hundredths of a second\n" \
"mtim <mins> <increment>         update computers time in minutes\n" \
"level x <mins> <inc>            xboard compat command\n" \
"load <file>                     load a saved game\n" \
"save <file>                     save a game\n" \
"quit                            quit!\n" \
"demo <1|0>                      enable/disable demo mode\n" \
"seek <1|0>                      enable/disable seeks (ics)\n" \
"answer <1|0>                    enable/disable seek answering (ics)\n" \
"ponder <1|0>                    enable/disable thinking on oppenents time\n" \
"robot <1|0>                     enable/disable ICS robot\n" \
"autoplay <1|0>                  enable/disable ICS autoplay\n" \
"undo <ply>                      take back <ply> half moves\n" \
"redo <ply>                      replay <ply> half moves\n" \
"eval                            static evaluation debug\n" \
"ppn                             display the ppn of the current position\n" \
"ppnset                          set the current position to a ppn\n" \
"espeed  <loops>                 measure eval fn speed\n" \
"testfin <file>                  load a .fin test file and test each position\n" \
"print                           print the current board\n" \
"about                           show some info on KnightCap\n" \
"target <opponent>               try challenging <opponent> when bored\n" \
"notarget                        undo the target command\n" \
"weakness <val>                  set weakness to val\n" \
"dementia <val>                  set dementia to val\n" \
"maxdepth <val>                  set max search depth\n" \
"maxmtime <val>                  set maximum move time to val\n" \
"mm  <n>                         play n games against another computer\n" \
"color <1|0>                     enable/disable color ascii board display\n" \
"bclean                          clean the brain\n" \
"buildbook <booksrc> <out>       build a book file\n" \
"dump <file> <scale>             dump a coefficients file\n" \
"");

}


int parse_stdin(char *line)
{
	char *p;
	Move move;
	char tok[1000];
	int hsecs, level, mins;
	static int increment;
	char file1[100];
	char file2[100];
	int on, i;
	int done = 0;
	float float1;

	p = line;

	while (next_token(&p, tok, "\n\r")) {
		/* is it a move? */
		if (parse_move(tok, &state->position, &move)) {
			if (player_moved(move.from, move.to))
				done = 1;
		}

		if (strcmp(tok,"go") == 0) {
			state->computer = next_to_play(&state->position);
			done = 1;
		}

		if (strcmp(tok,"black") == 0) {
			state->computer = -1;
			done = 1;
		}

		if (strcmp(tok,"white") == 0) {
			state->computer = 1;
			done = 1;
		}

		if (strcmp(tok,"new") == 0) {
			reset_board();
			redraw();
			done = 1;
		}

		if (sscanf(tok, "time %d", &hsecs) == 1) {
		  	state->ics_time = hsecs;
			timer_estimate(hsecs/100, increment);
			done = 1;
		}

		if (sscanf(tok, "mtim %d %d", &hsecs, &increment) == 2) {
			timer_estimate((60*hsecs), increment);
			done = 1;
		}


		if (sscanf(tok, "otim %d", &hsecs) == 1) {
		        state->ics_otim = hsecs;
			if (hsecs <= 0)
				lprintf(0, "win on time!\n");
			done = 1;
		}

		if (sscanf(tok, "level %d %d %d", &level, &mins, &increment) == 3) {
			lprintf(0,"setting increment to %d\n", increment);
			if (opponent)
				prog_printf("level %d %d %d\n", 
					    level, mins, increment);
			done = 1;
		}

		if (sscanf(tok, "testfin %50s", file1) == 1) {
			test_fin(file1);
			done = 1;
		}


		if (sscanf(tok, "load %50s", file1) == 1) {
			restore_game(file1);
			done = 1;
		}

		if (sscanf(tok, "save %50s", file1) == 1) {
			save_game(file1);
			done = 1;
		}

		if (sscanf(tok, "buildbook %50s %50s", file1, file2) == 2) {
			book_build(file1, file2);
			done = 1;
		}

		if (strncmp(tok, "target ", 7) == 0) {
			strncpy(play_target, tok+7, sizeof(play_target)-1);
			play_target[sizeof(play_target)-1] = 0;
			done = 1;
		}

		if (strcmp(tok, "notarget") == 0) {
			play_target[0] = 0;
			done = 1;
		}

		if (sscanf(tok, "weakness %lf", &state->eval_weakness) == 1) {
			done = 1;
		}

		if (sscanf(tok, "dementia %lf", &state->dementia) == 1) {
			done = 1;
		}

		if (sscanf(tok, "maxdepth %u", &state->max_search_depth) == 1) {
			done = 1;
		}

		if (sscanf(tok, "maxmtime %lf", &state->max_move_time) == 1) {
			done = 1;
		}

		if (strcmp(tok, "quit") == 0) {
			state->quit = 1;
			state->stop_search = 2;
			prog_printf("quit\n");
			prog_exit();
			exit(0);			
		}

		if (strcmp(tok, "print") == 0) {
			print_board(state->position.board);
			done = 1;
		}

		if (sscanf(tok,"demo %d", &on) == 1) {
			demo_mode = on;
			done = 1;
		}

		if (sscanf(tok,"seek %d", &on) == 1) {
			seeking = on;
			done = 1;
		}

		if (sscanf(tok,"answer %d", &on) == 1) {
			seek_answering = on;
			done = 1;
		}

		if (sscanf(tok,"color %d", &on) == 1) {
			color_display = on;
			done = 1;
		}

		if (sscanf(tok,"mm %d", &match_remaining) == 1) {
			match_points = 0;
			match_total = 0;
			done = 1;
		}

		if (sscanf(tok,"robot %d", &on) == 1) {
			state->ics_robot = on;
			done = 1;
		}

		if (sscanf(tok,"autoplay %d", &on) == 1) {
			state->autoplay = on;
			done = 1;
		}

		if (sscanf(tok,"display %d", &on) == 1) {
			no_display = !on;
			done = 1;
		}

		if (sscanf(tok,"mull %d", &on) == 1) {
			state->use_mulling = on;
			if (opponent == KNIGHTCAP)
				prog_printf("mull %d\n", on);
			done = 1;
		}

		if (sscanf(tok,"pbrain %d", &on) == 1) {
			state->use_pbrain = on;
			done = 1;
		}

		if (sscanf(tok,"ponder %d", &on) == 1) {
			always_think = state->always_think = on;
			if (opponent == CRAFTY) {
				if (on == 1) {
					prog_printf("ponder on\n");
				} else if (on == 0) {
					prog_printf("ponder off\n");
				}
			} else if (opponent == KNIGHTCAP) {
				prog_printf("ponder %d\n", on);
			}
			done = 1;
		}

		if (sscanf(tok,"mtime %d", &i) == 1) {
			state->move_time = i;
			if (opponent == CRAFTY)
				prog_printf("st %d\n", i);
			else if (opponent == KNIGHTCAP)
				prog_printf("mtime %d\n", i);
			done = 1;
		}

		if (sscanf(tok,"undo %d", &i) == 1) {
			undo_menu(i);
			done = 1;
		}

		if (sscanf(tok,"redo %d", &i) == 1) {
			undo_menu(-i);
			done = 1;
		}

		if (strcmp(tok, "eval") == 0) {
			eval_debug(&state->position);
			done = 1;
		}

		if (strcmp(tok, "ppn") == 0) {
			lprintf(0,"%s\n", position_to_ppn(&state->position));
			done = 1;
		}

		if (strncmp(tok, "ppnset", 6) == 0) {
			char *p2 = strchr(tok,' ');
			if (p2) {
				while ((*p2) == ' ') p2++;
				ppn_to_position(p2, &state->position);
			}
			done = 1;
		}

		if (strcmp(tok, "help") == 0) {
			help();
			done = 1;
		}

		if (strcmp(tok, "about") == 0) {
			about();
			done = 1;
		}

		if (strcmp(tok, "bclean") == 0) {
			brain_clean();
			done = 1;
		}

		if (sscanf(tok, "dump %50s %f", file1, &float1) == 2) {
			dump_coeffs(file1, float1);
			done = 1;
		}

#if STORE_LEAF_POS
		if (sscanf(tok, "booklearn %50s", file1) == 1) {
			book_learn(file1);
			done = 1;
		}
#endif
	}

	return done;
}


/* users can send us moves and commands on stdin */
static void check_stdin(void)
{
	char line[200];
	int n;
	fd_set set;
	struct timeval tval;
	static int nostdin;

	if (nostdin)
		return;

	FD_ZERO(&set);
	FD_SET(0, &set);

	tval.tv_sec = 0;
	tval.tv_usec = 0;

	if (select(1, &set, NULL, NULL, &tval) != 1)
		return;

	n = read(0, line, sizeof(line)-1);

	if (n <= 0) {
		lprintf(0,"disabling stdin\n");
		nostdin = 1;
		close(0);
		return;
	}

	line[n] = 0;

	if (!parse_stdin(line)) {
		if (prog_running()) {
			prog_printf("%s", line);
		}
	}
}


void update_display(void)
{
	if (!no_display) {
		if (ascii_display)
			print_board(state->position.board);
		else
			draw_all();
	}
}


void match_hook(void)
{
	static int player = 1;

	if (match_remaining <= 0) return;

	/* perhaps we are already playing */
	if (state->computer && !state->position.winner) return;

	if (state->position.winner) {
		if (state->position.winner == STALEMATE) {
			state->won = STALEMATE;
			match_points += 0.5;
		} else if (state->position.winner == player) {
			match_points += 1;
			state->won = 1;
		} else 
			state->won = 0;
		
		state->position.winner = 0;

		match_total++;
		match_remaining--;

		lprintf(0,"match %1.1f points out of %d games\n", 
			match_points, match_total);

		player = -player;

	}


	if (match_remaining <= 0) return;
	
	state->computer = 0;

	/* Give time to analyse last game */
	if (state->use_mulling) {
		sleep(30);
	} else {
#if LEARN_EVAL
		td_update();
		td_dump("coeff.dat");
#endif
	}
	reset_board();
	prog_printf("new\n");
	if (player == 1) {
		if (opponent == CRAFTY) 
			prog_printf("white\n");
		else if (opponent == KNIGHTCAP)
			prog_printf("black\n");
		state->computer = 1;
	} else {
		if (opponent == CRAFTY) {
			prog_printf("white\n");
			prog_printf("go\n");
		} else if (opponent == KNIGHTCAP)
			prog_printf("white\n");
		state->computer = -1;
	}
}


static void match_target(char *target)
{
#define MAX_TARGETS 10
	char *targets[MAX_TARGETS];
	int i;
	char *tok, *ptr=NULL;

	target = strdup(target);
	for (i=0, tok=strtok_r(target, " ", &ptr); 
	     tok && i<MAX_TARGETS;
	     tok = strtok_r(NULL, " ", &ptr)) {
		targets[i++] = tok;
	}

	if (i > 0) {
		prog_printf("match %s\n", targets[random() % i]);
	}

	free(target);
}


void idle_func(void)
{
	Move m1;
	static uint32 hash;
	static time_t seek_time;
	static time_t refresh_time;
	time_t t;

	t = time(NULL);

	check_stdin();

	if (prog_check_move(&m1, next_to_play(&state->position))) {
		player_moved(m1.from, m1.to);
	}

	if (state->position.winner && auto_exit) {
		FILE *f = fopen("results","a");
		float res;

		if (state->computer == state->position.winner)
			res = 1;
		else if (state->position.winner == STALEMATE)
			res = 0.5;
		else
			res = 0;
		if (f) {
			fprintf(f,"%g\n", res);
			fclose(f);
		}
		exit(0);
	}

	if (state->moved) {
		if (next_to_play(&state->position) == state->computer) {
			m1 = state->game_record[state->position.move_num];
			if (legal_move(&state->position, &m1)) {
				prog_tell_move(&state->position, &m1);
				display_move(&m1);
				if (!state->ics_robot &&
				    (state->position.flags & FLAG_ACCEPT_DRAW))
					prog_printf("draw\n");
				if (state->position.winner) {
					state->computer = 0;
					if (state->position.winner == STALEMATE)
						prog_printf("draw\n");
					if (state->ics_robot) {
						/* make sure we are right! */
						prog_printf("refresh\n");
					}
				}
				if (!state->ics_robot && !demo_mode)
					save_game("current.save");
			}
			state->stop_search = 0;
		}
		state->moved = 0;
	}

	if (need_redraw || hash != state->position.hash1) {
		need_redraw = 0;
		hash = state->position.hash1;
		update_display();
	}

	if (demo_mode && !state->position.winner &&
	    next_to_play(&state->position) != state->computer) {
		state->stop_search = 1;
		state->computer = next_to_play(&state->position);
	}

	if (!process_exists(move_pid) ||
	    (state->ics_robot && !prog_running())) {
			state->quit = 1;
			state->stop_search = 2;
			prog_printf("quit\n");
			prog_exit();
			exit(0);					
	}

	if (t > seek_time + SEEK_WAIT_TIME) {
		if (state->ics_robot && 
		    state->computer == 0 && seeking && state->autoplay) {
			prog_script("seeks.scr");
			seek_time = t;
		}
	}

	if (t > refresh_time + REFRESH_TIME) {
		if (state->ics_robot) {
			prog_printf("refresh\n");
			refresh_time = t;
			if (*play_target) {
				match_target(play_target);
			}
		}
	}

	match_hook();

	usleep(SLEEP_TIME);
}


void save_game(char *fname)
{
	int fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd == -1) {
		perror(fname);
		return;
	}
	write(fd, state, sizeof(*state));
	close(fd);
}

void restore_game(char *fname)
{
	int fd = open(fname, O_RDONLY);
	if (fd == -1) {
		perror(fname);
		return;
	}
	read(fd, state, sizeof(*state));
	close(fd);
}


/* undo the last n ply of the game 
 Passing a negative number gives redo */
void undo_menu(int n)
{
	int i;

	state->computer = 0;
	state->stop_search = 1;
	demo_mode = 0;

	n = state->position.move_num - n;
	if (n < 0) n = 0;

	setup_board(&state->position);

	for (i=0;i<n;i++) {
		Move *move = &state->game_record[i];
		if (!legal_move(&state->position, move))
			break;
		do_move(&state->position, &state->position, move);
	}

	state->stop_search = 1;

	redraw();
}

static void move_thread(void)
{
	int pondered = 0;
	unsigned mull_counter=0;

	lprintf(0,"started move_thread\n");

	init_hash_table();
	
#if TPROF
	tprof_init();
#endif

#if 1
	chdir("prof");
#endif

#if APLINUX
	if (getcid() != 0) {
		move_slave();
		exit();
	}
#endif

	while (!state->quit) {
		Move move;
		static int initialised;

		if (!initialised && (tune_nps == 0 || state->tuned)) {
			initialised = 1;
#if USE_PBRAIN
			brain_open(BRAIN_FILE);
#endif

#if USE_BOOK
			book_open(BOOK_FILE);
#endif
		}


		if (!process_exists(display_pid)) {
#if USE_PBRAIN
			brain_close();
#endif
			exit(0);					
		}

		if (state->need_reset) {
			hash_reset();
			order_reset();
			state->need_reset = 0;
		}

		if (state->computer == 0 && state->use_mulling) {
			/* if we get bored waiting then start to mull on
			   our brain */
			analyse_game();
			if (mull_counter++ > 
			    MULL_WAIT_TIME*(1000*1000/SLEEP_TIME)) {
#if BOOK_LEARN
				book_learn("booklearn.dat");
#else
				brain_mull();
#endif
			}
		} else {
			mull_counter = 0;
		}

		if (next_to_play(&state->position) != state->computer ||
		    state->moved || state->position.winner) {
			if (state->always_think && 
			    !state->position.winner &&
			    !pondered &&
			    state->computer == -next_to_play(&state->position)) {
				zero_move(&move);
				ponder_move(&state->position, &move);
				if (!is_zero_move(&move) &&
				    legal_move(&state->position, &move)) {
					state->game_record[state->position.move_num] = move;
					state->moved = 1;
					wake_up();
					pondered = 0;
				} else {
					pondered = 1;
				}
			}
			usleep(SLEEP_TIME);
			continue;
		}

		usleep(SLEEP_TIME);

		pondered = 0;

		if (make_move(&state->position, &move)) {
			if (next_to_play(&state->position) == state->computer &&
			    legal_move(&state->position, &move)) {
				state->game_record[state->position.move_num] = move;
				state->moved = 1;
				wake_up();
			}
		} else {
			state->computer = 0;
		}
	}

#if USE_PBRAIN
	brain_close();
#endif

#if TPROF
	tprof_end();
#endif

	sleep(1);

	exit(0);
}


static void create_threads(void)
{
	int len;
	int pid1, pid2;

	len = sizeof(*state);

	state = (struct state *)shm_allocate(len);

	state->move_time = default_move_time;
	state->use_mulling = 1;
	state->use_pbrain = 1;

	state->ics_robot = ics_mode;
	state->autoplay = ics_mode;
	state->hash_table_size = def_hash_table_size;
#if USE_SMP
	state->num_cpus = def_cpus;
#endif

	reset_board();

	init_movements();

	signal(SIGCLD, SIG_IGN);

	pid1 = getpid();

#ifdef APLINUX
	move_pid = getpid();
	if (getcid() != 0 || fork() != 0) {
		log_close();
		move_thread();
		exit(0);
	}
#else
	if ((pid2=fork()) != 0) {
		if (getpid() == pid1) {
			display_pid = pid2;
		} else {
			display_pid = pid1;
		}
		move_thread();
		exit(0);
	}
#endif

	if (getpid() == pid1) {
		move_pid = pid2;
	} else {
		move_pid = pid1;
	}
}


static void usage(void)
{
	printf("\n" \
"-w <window size>\n" \
"-c <external chess program>\n" \
"-s <seed>\n" \
"-n                  no display\n" \
"-a                  ascii display\n" \
"-B                  b&w ascii display\n" \
"-e                  issue seeks in ICS mode\n" \
"-z                  answer computer seeks in ICS mode\n" \
"-d                  demo mode\n" \
"-m <n>              play n games against another computer\n"\
"-I <icsname>        ICS mode\n" \
"-M <icsmaster>      set ICS master\n" \
"-t <time>           default move time\n" \
"-X                  catch sigint for xboard\n" \
"-A                  always think\n" \
"-H <size>           set hash table size in MB\n" \
"-f <file>           get eval coefficients from this file\n" \
"-y                  don't mull\n" \
"\n");
}


static void parse_options(int argc,char *argv[])
{
	FILE *f;
#ifdef SUNOS4
	extern char *optarg;
#endif
	char *opt = "T:c:P:s:w:nI:t:hXAD:l:aH:M:ezdm:f:ByE";
	int c;
	int seed = time(NULL);

	while ((c = getopt(argc, argv, opt)) != EOF) {
		switch (c) {
		case 'D':
			{
				char disp[100];
				sprintf(disp,"DISPLAY=%s", optarg);
				putenv(disp);
				printf("display set to %s\n", getenv("DISPLAY"));
			}
			break;

		case 'w':
#if APLINUX
			if (getcid()) break;
#endif
			window_size = atoi(optarg);
			break;

		case 'c':
#if APLINUX
			if (getcid()) break;
#endif
			prog_start(optarg);
			prog_script("login.scr");
			if (strstr(optarg, "crafty")) {
				opponent = CRAFTY;
				lprintf(0, "Playing crafty\n");
			}
			if (strstr(optarg, "KnightCap")) {
				opponent = KNIGHTCAP;
				lprintf(0, "Playing KnightCap\n");
			}
			break;

		case 'H':
			def_hash_table_size = atoi(optarg);
			break;

		case 's':
			seed = atoi(optarg);
			break;

		case 'P':
			def_cpus = atoi(optarg);
			break;

		case 'n':
			no_display = 1;
			break;

		case 'E':
			auto_exit = 1;
			break;

		case 'a':
			ascii_display = 1;
			break;

		case 'e':
			seeking = 1;
			break;

		case 'z':
			seek_answering = 1;
			f = fopen("opponents.dat", "r");
			if (f) {
				fscanf(f, "%s", opponents_list);
			}
			fclose(f);
			f = fopen("noplay.dat", "r");
			if (f) {
				fscanf(f,"%s", noplay_list);
			}	
			fclose(f);
			lprintf(0, "answering computer seeks %s %s\n",
				opponents_list, noplay_list);
			break;

		case 'd':
			demo_mode = 1;
			break;

		case 'y':
			use_mulling = 0;
			break;

		case 'm':
			match_remaining = atoi(optarg);
			lprintf(0,"%d game match\n", match_remaining);
			match_points = 0;
			match_total = 0;
			break;

		case 'B':
			color_display = 0;
			ascii_display = 1;
			break;
				
		case 'f':
			coeffs_file = optarg;
			break;

		case 'I':
			ics_mode = 1;
			ics_name = optarg;
			break;

		case 'M':
			ics_master = optarg;
			break;

		case 'T':
			tune_nps = atoi(optarg);
			break;

		case 't':
			default_move_time = atoi(optarg);
			if (opponent == CRAFTY)
				prog_printf("st %d\n", (int)default_move_time);
			else if (opponent == KNIGHTCAP)
				prog_printf("mtime %d\n", (int)default_move_time);
			break;

		case 'h':
			usage();
			exit(0);
			break;

		case 'X':
			signal(SIGINT, SIG_IGN);
			break;

		case 'A':
			always_think = 1;
			break;
		}
	}

	srandom(seed); 
}


static void nodisp_loop(void)
{
	while (1) {
		idle_func();
		usleep(100000);
	}
}

int main(int argc,char *argv[])
{
#ifdef APLINUX
	ap_init();
#endif

	setlinebuf(stdout);

	lprintf(0,"KnightCap starting\ntype \"help\" for commands or use the right button\n");

#if LEARN_EVAL
	if ((bad_eval_fd = open(bad_eval_file, O_RDWR)) == -1) {
		lprintf(0,"Failed to open %s\n", bad_eval_file);
	}
#endif

	parse_options(argc, argv);

	create_threads();

	reset_board();
	init_eval_tables();
	sig_wake_up();

	if (tune_nps) {
		autotune(tune_nps);
	}

#if RENDERED_DISPLAY
	if (!no_display && !ascii_display) {
		start_display(argc, argv);
		return 0;
	}
#endif

	nodisp_loop();

	return 0;
}
