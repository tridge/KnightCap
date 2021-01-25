/* the movement map generator - this allows you to quickly determine 
   if a particular piece type can ever capture from one square to another.

   This is useful for Xray attacks in the eval fn
 */

#include "includes.h"
#include "knightcap.h"

char capture_map[2*KING+1][NUM_SQUARES][NUM_SQUARES];
char sliding_map[NUM_SQUARES][NUM_SQUARES];
char edge_square[NUM_SQUARES];
uint64 same_line_mask[NUM_SQUARES][NUM_SQUARES];

static void init_pawn_movements(Piece p)
{
	int from;

	for (from=A1;from<=H8;from++) {
		if (YPOS(from) == 0 || YPOS(from) == 7) continue;
		if (p == PAWN) {
			if (XPOS(from) != 0)
				capture_map[p+KING][from][from+NORTH_WEST] = 1;
			if (XPOS(from) != 7)
				capture_map[p+KING][from][from+NORTH_EAST] = 1;
		} else {
			if (XPOS(from) != 0)
				capture_map[p+KING][from][from+SOUTH_WEST] = 1;
			if (XPOS(from) != 7)
				capture_map[p+KING][from][from+SOUTH_EAST] = 1;
		}
	}

}

static void init_knight_movements(Piece p)
{
	int from, to;

	for (from=A1;from<=H8;from++)
		for (to=A1;to<=H8;to++) {
			int dx = abs(XPOS(from) - XPOS(to));
			int dy = abs(YPOS(from) - YPOS(to));
			if (from == to) continue;
			if ((dx == 1 && dy == 2) || (dx == 2 && dy == 1)) {
				capture_map[p+KING][from][to] = 1;
			}
		}
}

static void init_bishop_movements(Piece p)
{
	int from, to;

	for (from=A1;from<=H8;from++)
		for (to=A1;to<=H8;to++) {
			int dx = abs(XPOS(from) - XPOS(to));
			int dy = abs(YPOS(from) - YPOS(to));
			if (from == to) continue;
			if (dx == dy) {
				capture_map[p+KING][from][to] = 1;
			}
		}
}

static void init_rook_movements(Piece p)
{
	int from, to;

	for (from=A1;from<=H8;from++)
		for (to=A1;to<=H8;to++) {
			int dx = abs(XPOS(from) - XPOS(to));
			int dy = abs(YPOS(from) - YPOS(to));
			if (from == to) continue;
			if (dx == 0 || dy == 0) {
				capture_map[p+KING][from][to] = 1;
			}
		}
}

static void init_queen_movements(Piece p)
{
	int from, to;

	for (from=A1;from<=H8;from++)
		for (to=A1;to<=H8;to++) {
			int dx = abs(XPOS(from) - XPOS(to));
			int dy = abs(YPOS(from) - YPOS(to));
			if (from == to) continue;
			if ((dx == 0 || dy == 0) || (dx == dy)) {
				capture_map[p+KING][from][to] = 1;
			}
		}
}


static void init_king_movements(Piece p)
{
	int from, to;

	for (from=A1;from<=H8;from++)
		for (to=A1;to<=H8;to++) {
			int dx = abs(XPOS(from) - XPOS(to));
			int dy = abs(YPOS(from) - YPOS(to));
			if (from == to) continue;
			if (dx <= 1 && dy <= 1) {
				capture_map[p+KING][from][to] = 1;
			}
		}
}


static void init_sliding_map(void)
{
	Square p1, p2, s1, s2;	
	int dir, i;
	memset(sliding_map,0, sizeof(sliding_map));
	
	for (i=0;i<8;i++) {
		dir = EAST;
		p1 = A1 + i*NORTH;
		p2 = H1 + i*NORTH;
		for (s1=p1;s1<p2;s1+=dir)
			for (s2=s1+dir;s2<=p2;s2+=dir) {
				sliding_map[s1][s2] = dir;
				sliding_map[s2][s1] = -dir;
			}
	}

	for (i=0;i<8;i++) {
		dir = NORTH;
		p1 = A1 + i*EAST;
		p2 = A8 + i*EAST;
		for (s1=p1;s1<p2;s1+=dir)
			for (s2=s1+dir;s2<=p2;s2+=dir) {
				sliding_map[s1][s2] = dir;
				sliding_map[s2][s1] = -dir;
			}
	}

	for (i=0;i<8;i++) {
		dir = NORTH_EAST;
		p1 = A8 + i*SOUTH;
		p2 = A8 + i*EAST;
		for (s1=p1;s1<p2;s1+=dir)
			for (s2=s1+dir;s2<=p2;s2+=dir) {
				sliding_map[s1][s2] = dir;
				sliding_map[s2][s1] = -dir;
			}
	}

	for (i=1;i<8;i++) {
		dir = NORTH_EAST;
		p1 = A1 + i*EAST;
		p2 = H8 + i*SOUTH;
		for (s1=p1;s1<p2;s1+=dir)
			for (s2=s1+dir;s2<=p2;s2+=dir) {
				sliding_map[s1][s2] = dir;
				sliding_map[s2][s1] = -dir;
			}
	}


	for (i=0;i<8;i++) {
		dir = SOUTH_EAST;
		p1 = A1 + i*NORTH;
		p2 = A1 + i*EAST;
		for (s1=p1;s1<p2;s1+=dir)
			for (s2=s1+dir;s2<=p2;s2+=dir) {
				sliding_map[s1][s2] = dir;
				sliding_map[s2][s1] = -dir;
			}
	}

	for (i=1;i<8;i++) {
		dir = SOUTH_EAST;
		p1 = A8 + i*EAST;
		p2 = H1 + i*NORTH;
		for (s1=p1;s1<p2;s1+=dir)
			for (s2=s1+dir;s2<=p2;s2+=dir) {
				sliding_map[s1][s2] = dir;
				sliding_map[s2][s1] = -dir;
			}
	}
}

static void init_same_line_mask(void)
{
	Square p1, p2;
	int s1, s2;	
	int dir, i;
	uint64 mask;

	memset(same_line_mask,0, sizeof(same_line_mask));
	
	for (i=0;i<8;i++) {
		dir = EAST;
		p1 = A1 + i*NORTH;
		p2 = H1 + i*NORTH;

		mask = 0;
		for (s1=p1; s1<=p2; s1+=dir)
			mask |= ((uint64)1<<s1);

		for (s1=p1;s1<=p2;s1+=dir)
			for (s2=p1;s2<=p2;s2+=dir)
				if (s1 != s2) {
					same_line_mask[s1][s2] = mask;
				}
	}

	for (i=0;i<8;i++) {
		dir = NORTH;
		p1 = A1 + i*EAST;
		p2 = A8 + i*EAST;

		mask = 0;
		for (s1=p1; s1<=p2; s1+=dir)
			mask |= ((uint64)1<<s1);

		for (s1=p1;s1<=p2;s1+=dir)
			for (s2=p1;s2<=p2;s2+=dir)
				if (s1 != s2) {
					same_line_mask[s1][s2] = mask;
				}
	}

	for (i=0;i<8;i++) {
		dir = NORTH_EAST;
		p1 = A8 + i*SOUTH;
		p2 = A8 + i*EAST;

		mask = 0;
		for (s1=p1; s1<=p2; s1+=dir)
			mask |= ((uint64)1<<s1);

		for (s1=p1;s1<=p2;s1+=dir)
			for (s2=p1;s2<=p2;s2+=dir)
				if (s1 != s2) {
					same_line_mask[s1][s2] = mask;
				}
	}

	for (i=1;i<8;i++) {
		dir = NORTH_EAST;
		p1 = A1 + i*EAST;
		p2 = H8 + i*SOUTH;

		mask = 0;
		for (s1=p1; s1<=p2; s1+=dir)
			mask |= ((uint64)1<<s1);

		for (s1=p1;s1<=p2;s1+=dir)
			for (s2=p1;s2<=p2;s2+=dir)
				if (s1 != s2) {
					same_line_mask[s1][s2] = mask;
				}
	}


	for (i=0;i<8;i++) {
		dir = NORTH_WEST;
		p1 = A1 + i*EAST;
		p2 = A1 + i*NORTH;

		mask = 0;
		for (s1=p1; s1>=p2; s1+=dir)
			mask |= ((uint64)1<<s1);

		for (s1=p1;s1>=p2;s1+=dir)
			for (s2=p1;s2>=p2;s2+=dir)
				if (s1 != s2) {
					same_line_mask[s1][s2] = mask;
				}
	}

	for (i=1;i<8;i++) {
		dir = NORTH_WEST;
		p1 = H1 + i*NORTH;
		p2 = A8 + i*EAST;

		mask = 0;
		for (s1=p1; s1>=p2; s1+=dir)
			mask |= ((uint64)1<<s1);

		for (s1=p1;s1>=p2;s1+=dir)
			for (s2=p1;s2>=p2;s2+=dir)
				if (s1 != s2) {
					same_line_mask[s1][s2] = mask;
				}
	}
}


static void init_edges(void)
{
	int p;

	memset(edge_square, 0, sizeof(edge_square));

	for (p=A1;p<=H1;p+=EAST)
		edge_square[p] += SOUTH;

	for (p=A1;p<=A8;p+=NORTH)
		edge_square[p] += WEST;

	for (p=A8;p<=H8;p+=EAST)
		edge_square[p] += NORTH;

	for (p=H1;p<=H8;p+=NORTH)
		edge_square[p] += EAST;
}

void init_movements(void)
{
	static int initialised;

	if (initialised) return;
	initialised = 1;

	init_pawn_movements(PAWN);
	init_knight_movements(KNIGHT);
	init_bishop_movements(BISHOP);
	init_rook_movements(ROOK);
	init_queen_movements(QUEEN);
	init_king_movements(KING);

	init_pawn_movements(-PAWN);
	init_knight_movements(-KNIGHT);
	init_bishop_movements(-BISHOP);
	init_rook_movements(-ROOK);
	init_queen_movements(-QUEEN);
	init_king_movements(-KING);

	init_sliding_map();
	init_same_line_mask();
	init_edges();
}
