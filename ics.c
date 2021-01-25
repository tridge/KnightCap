#include "includes.h"
#include "knightcap.h"

extern struct state *state;

extern char opponents_list[];
extern char noplay_list[];
/*
Here are some samples of what FICS looks like:

fics% match 
jeromed (1601) seeking 2 12 unrated blitz ("play 17" to respond)
fics% ThingOne
Issuing: tridge (++++) ThingOne (2247) unrated blitz 2 12.
--** ThingOne is a computer **--
fics% 
ThingOne accepts your challenge.
fics% 
Creating: ThingOne (2247) tridge (++++) unrated blitz 2 12
{Game 34 (ThingOne vs. tridge) Creating unrated blitz match.}

#@#000tango           *tridge          :RNBQKBNRPPPP PPP            P                   pppppppprnbqkbnr001B39390011500120P/e2-e4(0:17)@#@



fics% 
Challenge: ixosjaps (1935) Tridge (2128) rated blitz 2 12.
Your blitz rating will change:  Win: +33,  Draw: -32,  Loss: -98
Your new RD will be 151.7
You can "accept" or "decline", or propose different parameters.
fics% acceptposition is size 224

You accept the challenge of ixosjaps.

Creating: Tridge (2128) ixosjaps (1935) rated blitz 2 12
{Game 14 (Tridge vs. ixosjaps) Creating rated blitz match.}

Game 14 (Tridge vs. ixosjaps)

#@#013Tridge          *ixosjaps        :RNBQKBNRPPPPPPPP                                pppppppprnbqkbnr001W39390012000120none   (0:00)@#@
fics% W: 20 n/s 105/0 2:2 mo=21/2 hash=0/105 t=0 d2d3
W: -12 n/s 639/0 3:4 mo=80/21 hash=65/641 t=0 d2d3


blindfolded accepts your challenge.
fics% 
Creating: blindfolded (1687) KnightCap (1992) rated blitz 2 10
{Game 6 (blindfolded vs. KnightCap) Creating rated blitz match.}

Game 6 (blinposition is size 228


White (KnightCap) : 2 mins, 16 secs
Black (mrb) : 1 mins, 34 secs

Notification: TayxBot, who has an adjourned game with you, has arrived.
fics% 
Challenge: TayxBot (1940) KnightCap (1940) rated blitz 2 10 (adjourned).
--** TayxBot is a computer **--
Your blitz rating will change:  Win: +4,  Draw: +0,  Loss: -4

{Game 52 (KlausK vs. KnightCap) KlausK resigns} 0-1

Tridge offers you a draw.


Tridge would like to abort the game; type "abort" to accept.

Tridge would like to adjourn the game; type "adjourn" to accept.

Miuek requests to pause the game.


Notification: tufnel, who has an adjourned game with you, has arrived

*/

extern char *ics_name;
extern char *ics_master;
extern char *play_target;
extern int seeking;
extern int seek_answering;

#define ICSNAME ics_name

#define ICSPROMPT1 "aics% "
#define ICSPROMPT2 "fics% "
#define ICSEND "} "
#define ICSDISCONNECT "disconnected"
#define ICSNOGAME1 "re neither playing,"
#define ICSNOGAME2 "re not observing or playing"
#define ICSADJOURN1 "requests adjournment"
#define ICSADJOURN2 "would like to adjourn the game"
#define ICSABORT1 "requests that the game be aborted"
#define ICSABORT2 "would like to abort the game"
#define ICSRATING1 "Rating changes:"
#define ICSRATING2 "rating will change:"
#define ICSGAMEOVER "{Game %d %s vs. %s %s %s"
#define NOPLAY "KnightC is learning its eval so it accepts seeks from other computers (computer play helps). But it never accepts against the same computer twice, so it will play you very infrequently. If you don't like this, please message KnightC and KnightC will stop playing you."

void ics_thanks(void)
{
	prog_printf("say thanks for the game\n");
	if (seeking && state->autoplay) {
		prog_script("seeks.scr");
	}		
}

static void parse_ics_special(char *line)
{
	char white_player[1000];
	char black_player[1000];
	char white_rating[1000], black_rating[1000];
	int game_num;
	char dum[1000], dum1[1000], dum2[1000];
	char result[1000], last_opponent[1000], 
		opponent[1000], last_last_opponent[1000];
	int increment, game_time;
	int temp_time, temp_inc, rating;
	static FILE *f;

	extern struct state *state;

	if (*line == '\r') line++;

	if (strlen(line) > 900) {
		lprintf(0,"hacker attack?? len=%d\n", strlen(line));
		return;
	}

	if (!state->ics_robot)
		return;

	if (!f) {
		f = fopen("ics.log", "w");
		if (!f)
			f = fopen("/tmp/ics.log", "w");
		setlinebuf(f);
	}

	if (ics_master && sscanf(line,"%s tells you: icsrun ", dum) == 1 &&
	    strstr(line," tells you: icsrun ") &&
	    strcmp(dum, ics_master) == 0) {
		char *p = strstr(line,"icsrun ");
		p += strlen("icsrun ");
		if (!parse_stdin(p)) {
			if (prog_running()) {
				prog_printf("%s\n",p);
			}
		}
		return;
	}

	/* answer computer seeks in ICS (probably doesn't work for FICS
	   right now) */
	if ((seek_answering) && !strstr(line, "KnightC") && 
	    strstr(line, "seeking") && 
	    (strstr(line, "blitz") || strstr(line, "bullet") || 
	     strstr(line, "Blitz") || strstr(line, "Lightning"))) {
		sscanf(line, "%s (%d) %s %s %d %d", opponent, &rating, 
		       dum1, dum2, &temp_time, &temp_inc);
		lprintf(0, "%s %s %s %d %d %d %d\n", last_last_opponent,
			last_opponent, opponent, 
			strcmp(opponent, last_opponent),
			strstr(noplay_list, opponent), rating, temp_time,
			temp_inc);
		if (strcmp(opponent, last_last_opponent) !=0 && 
		    strcmp(opponent, last_opponent) !=0 && 
		    strstr(noplay_list, opponent) == NULL && 
		    rating >= 2200 && 
		    temp_time < 10 && 
		    temp_inc < 15
		    && temp_time > 1) {
			int game_num;
			char *p = strstr(line, "play ");
			p += strlen("play ");
			sscanf(p, "%d", &game_num);
			prog_printf("play %d\n", game_num);
			lprintf(0,"challenged: %s\n", opponent);
			strcpy(last_last_opponent,last_opponent);
			strcpy(last_opponent,opponent);
			if (!strstr(opponents_list,opponent)) {
				FILE *f;

				prog_printf("mess %s %s\n", opponent, NOPLAY);
				strcat(opponents_list, opponent);
				f = fopen("opponents.dat", "w");
				fprintf(f, "%s", opponents_list);
				fclose(f);
			}
		}
	}

	/* ignore all other tells */
	if (strstr(line,"tells you:")) {
		return;
	}

	/* maybe we think we are playing, but we're not */
	if (strstr(line,ICSNOGAME1) || strstr(line,ICSNOGAME2)) {
		state->computer = 0;
		return;
	}

	/* someone offers us a draw? */
#if 0
	if (state->autoplay && strstr(line,"offers you a draw.")) {
		if (state->position.evaluation * state->computer < -PAWN_VALUE ||
		    (abs(state->position.evaluation) < 20 &&
		     state->position.flags & FLAG_ACCEPT_DRAW)) {
			prog_printf("draw\n");
		} else {
			prog_printf("say my programming says this is not a draw (it may be wrong)\n");
		}
		return;
	}
#endif

	/* someone wants to abort? only abort at the very start of a game */
	if (state->autoplay && 
	    (strstr(line,ICSABORT1) || strstr(line,ICSABORT2))) {
		if (state->position.move_num < 6) {
			prog_printf("abort\n");
		} else {
			prog_printf("say this computer only accepts aborts in the first 3 moves of a game\n");
		}
		return;
	}

	/* someone wants to pause? */
	if (state->autoplay &&
	    strstr(line,"requests to pause the game")) {
		prog_printf("pause\n");
		return;
	}

	/* someone wants to unpause? */
	if (state->autoplay &&
	    strstr(line,"requests to unpause the game")) {
		prog_printf("unpause\n");
		return;
	}

	/* someone wants to adjourn? always accept */
	if (state->autoplay &&
	    (strstr(line,ICSADJOURN1) || strstr(line,ICSADJOURN2))) {
		prog_printf("adjourn\n");
		return;
	}

	/* is it all over? */
	if (sscanf(line,ICSGAMEOVER, 
		   &game_num, white_player, black_player, dum, result) == 5 &&
	    strstr(line,ics_name) && !strstr(line,"Creating")) {
		if (strstr(line,"1-0") || strstr(line,"1/2-1/2") ||
		    strstr(line,"0-1")) {
			char *p = strstr(line, ICSEND);
			p += strlen(ICSEND);
			fprintf(f, "%s\n", p);
			ics_thanks();
			if (strstr(line,"checkmated")) {
				if ((strstr(line,"1-0") && 
				     strstr(white_player, ics_name)) || 
				    (strstr(line,"0-1") && 
				     strstr(black_player, ics_name))) 
					state->won = 1;
				else 
					state->won = 0;
			} else if (strstr(line, "draw")) {
				state->won = STALEMATE;
			} else {			
				state->won = TIME_FORFEIT;
			} 
		} else if (strstr(line, ICSDISCONNECT)) {
			fprintf(f,"%s\n", line);
		}
		state->computer = 0;
		return;
	}

	/* has someone challenged us? */
	if (sscanf(line,"Challenge: %s", dum) == 1) {
		if (state->autoplay) {
			/* auto accept! */
			prog_printf("accept %s\n", dum);
			fprintf(f, "accepted challenge\n");
		}
		return;
	}

	/* has someone with an adjourned game arrived? */
	if (strstr(line,"adjourned") && 
	    sscanf(line,"Notification: %s who has an adjourned game with you",
		   dum) == 1) {
		if (dum[strlen(dum)-1] == ',')
			dum[strlen(dum)-1] = 0;
		if (state->autoplay) {
			prog_printf("tell %s Shall we finish our adjourned game?\n",
				    dum);
			if (state->computer == 0)
				prog_printf("match %s\n",dum);
		}
		return;
	}

	/* has a match been created? */
	if (sscanf(line,"Creating: %s %s %s %s %s %s %d %d",
		   white_player, white_rating,
		   black_player, black_rating,
		   dum, dum,
		   &game_time, &increment) == 8) {

	        reset_board();
		timer_estimate(game_time*60, increment);

		prog_printf("kibitz Hello from KnightCap! (running in ICS robot mode)\n");
		fprintf(f, "Creating %s vs %s mt=%2.1f\n",
			white_player, black_player, state->move_time);

		prog_printf("re\n");
		return;
	}
	
	/* get the rating changes */
	if (strstr(line,ICSRATING1) || strstr(line,ICSRATING2)) {
		char *p = strstr(line, "Draw: ");
		p += strlen("Draw: ");
		if (sscanf(p, "%d", &(state->rating_change)) == 1) {
			lprintf(0, "***Rating Change: %d\n", 
				state->rating_change);
		} 
	}
}


int parse_ics_move(char *line,int player, Move *move, Piece *promotion)
{
	extern struct state *state;
	int nextplayer, x, y;
	char board[20][9]; /* its made too long to be less error prone */
	char tomove;
	int pawnfile, wshort, wlong, bshort, blong, fiftycount, game;
	char whiteplayer[100], blackplayer[100];
	int relation;
	int initial_time;
	int increment;
	int white_time, black_time;
	int movenum;
	char prevmove[20];
	Position b1, b2;
	Position *b = &state->position;
	int bad_move = 0;
	int wstrength, bstrength;
	int from, to;

	while (*line && !isprint(*line))
		line++;

	while (strncmp(line,ICSPROMPT1,strlen(ICSPROMPT1)) == 0)
		line += strlen(ICSPROMPT1);

	while (strncmp(line,ICSPROMPT2,strlen(ICSPROMPT2)) == 0)
		line += strlen(ICSPROMPT2);

	memset(board, 0, sizeof(board));

	if (sscanf(line,"<12> %s %s %s %s %s %s %s %s %c %d %d %d %d %d %d %d %s %s %d %d %d %d %d %d %d %d %s",
		   board[7], board[6], board[5], board[4], 
		   board[3], board[2], board[1], board[0], 
		   &tomove,
		   &pawnfile,
		   &wshort, &wlong,
		   &bshort, &blong,
		   &fiftycount,
		   &game,
		   whiteplayer, blackplayer,
		   &relation,
		   &initial_time,
		   &increment,
		   &wstrength,
		   &bstrength,
		   &white_time,
		   &black_time,
		   &movenum,
		   prevmove) != 27) {
		parse_ics_special(line);
		return 0;
	}

	if (tomove == 'W') {
		nextplayer = 1;
	} else if (tomove == 'B') {
		nextplayer = -1;	
	} else {
		return 0;
	}

	if (strcmp(ics_name, whiteplayer) == 0) {
	  state->ics_time = white_time;
	  state->ics_otim = black_time;
		timer_estimate(white_time, increment);
		if (black_time <= 0)
			prog_printf("flag\n");
		state->colour = 1;
	}

	if (strcmp(ics_name, blackplayer) == 0) {
	  state->ics_time = black_time;
	  state->ics_otim = white_time;
		timer_estimate(black_time, increment);
		if (white_time <= 0)
			prog_printf("flag\n");
		state->colour = -1;
	}

	if (tomove == 'W') {
		lprintf(0,"*%s: %d:%02d    %s %d:%02d    %d. ... %s    mtime=%2.1f\n", 
			whiteplayer, white_time/60, white_time%60,
			blackplayer, black_time/60, black_time%60,
			movenum, prevmove, state->move_time);
	} else {
		lprintf(0," %s: %d:%02d   *%s %d:%02d    %d. %s        mtime=%2.1f\n", 
			whiteplayer, white_time/60, white_time%60,
			blackplayer, black_time/60, black_time%60,
			movenum, prevmove, state->move_time);
	}

	if (!strncmp(prevmove, "none", 4)) {
		/* its a new game */
		reset_board();
		if (abs(relation) == 1 && state->ics_robot) {
			if (strcmp(whiteplayer, ics_name) == 0) {
				state->computer = 1;
			} else {
				state->computer = -1;
			}
		}
		return 0;
	} else if (!strncmp(prevmove, "o-o-o", 5)) {
		if (nextplayer < 0) {
			move->from = E1;
			move->to = C1;
		} else {
			move->from = E8;
			move->to = C8;
		}
	} else if (!strncmp(prevmove, "o-o", 3)) {
		if (nextplayer < 0) {
			move->from = E1;
			move->to = G1;
		} else {
			move->from = E8;
			move->to = G8;
		}
	} else {
		if (prevmove[1] != '/') {
			return 0;
		}
		
		if (prevmove[4] != '-') {
			return 0;
		}

		if (!parse_square(prevmove+2, &from) || 
		    !parse_square(prevmove+5, &to))
			return 0;

		move->from = from;
		move->to = to;
	}

	if (abs(relation) == 1 && state->ics_robot) {
		if (strcmp(whiteplayer, ics_name) == 0) {
			state->computer = 1;
		} else {
			state->computer = -1;
		}
	}

	if (!legal_move(&state->position, move) ||
	    nextplayer != -player) {
		bad_move = 1;
	} 

	memset(&b1, 0, sizeof(b1));

	for (x=0;x<8;x++)
		for (y=0;y<8;y++) {
			Piece p;
			p = charpiece(board[y][x]);
			if (islower(board[y][x])) p = -p;
			b1.board[POSN(x,y)] = p;
		}

	if (memcmp(b->board, b1.board, sizeof(b1.board)) == 0) {
		/* we already have the right position */
		return 0;
	}

	if (wshort)
		b1.flags |= WHITE_CASTLE_SHORT;
	if (bshort)
		b1.flags |= BLACK_CASTLE_SHORT;
	if (wlong)
		b1.flags |= WHITE_CASTLE_LONG;
	if (blong)
		b1.flags |= BLACK_CASTLE_LONG;
	
	if (pawnfile != -1) {
		if (nextplayer == -1)
			b1.enpassent = POSN(pawnfile, 2);
		else
			b1.enpassent = POSN(pawnfile, 5);
	}

	b1.fifty_count = fiftycount;
	
	if (!bad_move) {
		if (!do_move(&b2, b, move)) {
			bad_move = 1;
		}
		if (memcmp(b2.board, b1.board, sizeof(b1.board)) != 0) {
			bad_move = 1;
		}
	}

	if (bad_move) {
		/* assume ICS is right! We try to generate a valid
		   position from what its given us */
		state->computer = 0;

		(*b) = b1;

		b->move_num = ((movenum-1) * 2);
		if (nextplayer == -1) {
			b->move_num++;
		}

		memset(state->hash_list, 0, 
		       sizeof(state->hash_list[0])*b->move_num);

		/* now the tricky bit - getting the pboard and piece
		   list reasonable */
		create_pboard(b);

		lprintf(0,"generated position\n");
		prog_printf("refresh\n");
		return 0;
	}

	/* its all ok! */

	return 1;
}


