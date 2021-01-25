#include "includes.h"
#include "knightcap.h"


static int isrank(char c)
{
	return (c >= '1' && c <= '8');
}

static int isfile(char c)
{
	return (c >= 'a' && c <= 'h');
}

int parse_square(char *s, int *sq)
{
	if (s[0] < 'a' || s[0] > 'h' ||
	    s[1] < '1' || s[1] > '8') 
		return 0;

	*sq = POSN(s[0]-'a', s[1] - '1');
	return 1;
}

static int assume_piece;
static Move moves[MAX_MOVES];
static int num_moves;

static int parse_algebraic(char *s, Position *b, Move *move)
{
	int capture = 0;
	int piece = -1;
	int from = -1, to = -1, file = -1, rank = -1;
	int file_to = -1, rank_to = -1;
	int i, parsed=0, count, matched=0;

	if (isdigit(s[1]) && isfile(s[0])) {
		/* it must be a pawn move */
		if (!parse_square(s, &to))
			return 0;
		file = XPOS(to);
		piece = PAWN;
		parsed = 1;
	}

	if (!parsed && charpiece(s[0])) {
		if (islower(s[0]) && isfile(s[0]) && !assume_piece) {
			if ((isfile(s[1]) && abs(s[1]-s[0])==1) || 
			    (s[1]=='x' && isfile(s[2]) && abs(s[2]-s[0])==1))
				goto parse_pawn;
		}

		/* its a piece move */
		piece = charpiece(s[0]);
		if (s[1] == 'x') {
			capture = 1;
			if (!parse_square(s+2, &to))
				goto parse_pawn;
			parsed = 1;
		} else if (s[2] == 'x') {
			capture = 1;
			if (isfile(s[1])) {
				file = s[1] - 'a';
			} else if (isrank(s[1])) {
				rank = s[1] - '1';
			} else {
				goto parse_pawn;
			}
			if (!parse_square(s+3, &to))
				goto parse_pawn;
			parsed = 1;
		} else if (s[3] == 'x') {
			capture = 1;
			if (!parse_square(s+1, &from))
				goto parse_pawn;
			if (isupper(s[4])) {
				if (!parse_square(s+5, &to))
					goto parse_pawn;  
			} else {
				if (!parse_square(s+4, &to))
					goto parse_pawn;
			}
			parsed = 1;
		} else {
			/* its not a capture! */
			if (parse_square(s+3, &to)) {
				if (!parse_square(s+1, &from))
					goto parse_pawn;
			} else if (parse_square(s+2, &to)) {
				if (isfile(s[1])) {
					file = s[1] - 'a';
				} else if (isrank(s[1])) {
					rank = s[1] - '1';
				} else {
					goto parse_pawn;
				}
			} else if (!parse_square(s+1, &to)) {
				if (isfile(s[1])) {
					file_to = s[1] - 'a';
				} else {
					goto parse_pawn;
				}
			}
			parsed = 1;
		}
	} 

 parse_pawn:
	if (!parsed) {
		/* it must be a pawn capture */
		from = -1; to = -1; file = -1; rank = -1; 
		file_to = -1; rank_to = -1;
		piece = PAWN;
		capture = 1;
		if (s[1] == 'x') {
			file = s[0] - 'a';
			if (!parse_square(s+2, &to)) {
				if (isfile(s[2])) {
					file_to = s[2] - 'a';
				} else {
					return 0;
				}
			}
		} else {
			if ((strlen(s) > 3 && s[3] != '=') || 
			    !isfile(s[0]) || !isfile(s[1]))
				return 0;
			file = s[0] - 'a';
			file_to = s[1] - 'a';
			if (isdigit(s[2]))
				rank_to = s[2] - '1';
		}
	}

	if (blacks_move(b)) piece = -piece;

	count=0;

	for (i=0;i<num_moves;i++) {
		if (!legal_move(b, &moves[i])) continue;
		if (b->board[moves[i].from] != piece) continue;
		if (to != -1 && moves[i].to != to) continue;
		if (from != -1 && moves[i].from != from) continue;
		if (file != -1 && XPOS(moves[i].from) != file) continue;
		if (file_to != -1 && XPOS(moves[i].to) != file_to) continue;
		if (rank != -1 && YPOS(moves[i].from) != rank) continue;
		if (rank_to != -1 && YPOS(moves[i].to) != rank_to) continue;
		matched = i;
		count++;
	}

	if (count > 1) {
		lprintf(0,"ambiguous move\n");
		return 0;
	}

	if (count == 0 && !assume_piece) {
		assume_piece = 1;
		return parse_algebraic(s, b, move);
	}
	
	if (count == 0) return 0;

	(*move) = moves[matched];

	if (strlen(s)>3) {
		char *p = strchr(s,'=');
		if (p && charpiece(p[1])) {
			b->promotion = charpiece(p[1]);
		}
	}

	return 1;
}

int parse_move(char *s, Position *b, Move *move)
{
	char x1, x2, y1, y2, becomes;
	int ret;

	b->promotion = 0;
	num_moves = generate_moves(b, moves);

	/* maybe its castling */
	if (strcasecmp(s, "o-o")==0) {
		if (whites_move(b)) {
			move->from = E1;
			move->to = G1;
			return 1;
		} else {
			move->from = E8;
			move->to = G8;
			return 1;
		}
	}
	if (strcasecmp(s, "o-o-o")==0) {
		if (whites_move(b)) {
			move->from = E1;
			move->to = C1;
			return 1;
		} else {
			move->from = E8;
			move->to = C8;
			return 1;
		}
	}

	if (s[2] == '-')
		ret = sscanf(s,"%c%c-%c%c%c", &x1, &y1, &x2, &y2, &becomes);
	else
		ret = sscanf(s,"%c%c%c%c%c", &x1, &y1, &x2, &y2, &becomes);
	
	if (ret < 4 || 
	    x1 < 'a' || x1 > 'h' ||
	    x2 < 'a' || x2 > 'h' ||
	    y1 < '1' || y1 > '8' ||
	    y2 < '1' || y2 > '8') {
		assume_piece = 0;
		return parse_algebraic(s, b, move);
	}

	move->from = POSN(x1-'a', y1-'1');
	move->to = POSN(x2-'a', y2-'1');

	if (ret == 5 && (y1 == '2' || y1 == '7') && (y2 == '1' || y2 == '8')) {
		b->promotion = charpiece(becomes);
	}

	return 1;
}		
