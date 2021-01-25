/* a 3D chess display for KnightCap

   Andrew.Tridgell@anu.edu.au January 1997 */

#include "includes.h"
#include "knightcap.h"

extern struct state *state;
int need_redraw=1;

#if RENDERED_DISPLAY
#include <GL/glut.h>
#include "trackball.h"

extern int window_size;

#define SLEEP_TIME 100000 /* microseconds */


static int do_scaling;
static int do_motion;
static int mouse_moved;
extern int demo_mode;
static int downx, downy;

/* the pieces don't sit quite on the board - there is a
   small gap */
#define BOARDGAP 0.05

#define BOARD1(p) (state->position.board[p])
#define BOARD(x,y) BOARD1(POSN(x,y))

static float a_color[4] =      {0.75, 0.75, 0.55};
static float b_color[4] =      {0.4, 0.4, 0.6};

/* traditional white and black squares */
static float square_color_1[4] =      {0.7, 0.7, 0.7};
/* static float square_color_2[4] =      {0, 0, 0}; */

enum {MENU_RESET, MENU_QUIT, MENU_FLIP, MENU_RESET_VIEW, MENU_DEMO,
      MENU_COMPUTER_WHITE, MENU_COMPUTER_BLACK, MENU_SAVE, MENU_RESTORE,
      MENU_EVAL, MENU_SPEED};


enum {EDIT_NONE, EDIT_RECREATE, EDIT_CLEAR, EDIT_WHITES_MOVE,
      EDIT_BLACKS_MOVE,
      EDIT_BPAWN, EDIT_BKNIGHT, EDIT_BBISHOP,
      EDIT_BROOK, EDIT_BQUEEN, EDIT_BKING, 
      EDIT_WPAWN, EDIT_WKNIGHT, EDIT_WBISHOP,
      EDIT_WROOK, EDIT_WQUEEN, EDIT_WKING};



#define NUM_SLICES 10
#define NUM_STACKS 8

static int menu_ics_robot, menu_always_think, menu_demo_mode;

void redraw(void)
{
	need_redraw = 1;
}

static void set_cursor(int c)
{
	static int last_c;
	if (last_c == c) return;
	glutSetCursor(c);
	last_c = c;
}

/* this is taken from the glut library */
static void drawBox(GLfloat x0, GLfloat x1, GLfloat y0, GLfloat y1,
		    GLfloat z0, GLfloat z1)
{
	static GLfloat n[6][3] =
	{
		{-1.0, 0.0, 0.0},
		{0.0, 1.0, 0.0},
		{1.0, 0.0, 0.0},
		{0.0, -1.0, 0.0},
		{0.0, 0.0, 1.0},
		{0.0, 0.0, -1.0}
	};
	static GLint faces[6][4] =
	{
		{0, 1, 2, 3},
		{3, 2, 6, 7},
		{7, 6, 5, 4},
		{4, 5, 1, 0},
		{5, 6, 2, 1},
		{7, 4, 0, 3}
	};
	GLfloat v[8][3];
	GLint i;
	
	v[0][0] = v[1][0] = v[2][0] = v[3][0] = x0;
	v[4][0] = v[5][0] = v[6][0] = v[7][0] = x1;
	v[0][1] = v[1][1] = v[4][1] = v[5][1] = y0;
	v[2][1] = v[3][1] = v[6][1] = v[7][1] = y1;
	v[0][2] = v[3][2] = v[4][2] = v[7][2] = z0;
	v[1][2] = v[2][2] = v[5][2] = v[6][2] = z1;
	
	for (i = 0; i < 6; i++) {
		glBegin(GL_QUADS);
		glNormal3fv(&n[i][0]);
		glVertex3fv(&v[faces[i][0]][0]);
		glVertex3fv(&v[faces[i][1]][0]);
		glVertex3fv(&v[faces[i][2]][0]);
		glVertex3fv(&v[faces[i][3]][0]);
		glEnd();
	}
}

static void draw_pawn(void)
{
	GLUquadricObj *q;
	q = gluNewQuadric();
	gluCylinder(q, 0.22, 0.1, 0.65, NUM_SLICES, NUM_STACKS);
	glTranslatef(0,0,0.6);
	glutSolidSphere(0.14,NUM_SLICES,NUM_STACKS);
}

static void draw_rook(void)
{
	GLUquadricObj *q;

	/* the base */
	drawBox(-0.25, 0.25, -0.25, 0.25, 0, 0.1);
	glTranslatef(0,0,0.1);

	/* the torso */
	q = gluNewQuadric();
	gluCylinder(q, 0.22, 0.16, 0.5, NUM_SLICES, NUM_STACKS);
	glTranslatef(0, 0, 0.5);

	/* the top */
	drawBox(-0.2, 0.2, -0.2, 0.2, 0, 0.1);
	glTranslatef(0,0,0.1);

	glTranslatef(-0.15, -0.15, 0.05);
	glutSolidCube(0.1);
	glTranslatef(0.3, 0, 0);
	glutSolidCube(0.1);
	glTranslatef(0, 0.3, 0);
	glutSolidCube(0.1);
	glTranslatef(-0.3, 0, 0);
	glutSolidCube(0.1);
}

static void draw_bishop(void)
{
	GLUquadricObj *q;

	glScalef(1.2, 1.2, 1);

	/* the torso */
	q = gluNewQuadric();
	gluCylinder(q, 0.22, 0.12, 0.9, NUM_SLICES, NUM_STACKS);
	glTranslatef(0, 0, 0.85);

	/* the sash */
	glPushMatrix();
	glRotatef(-45, 1, 0, 0);
	glScalef(1, 1.5, 1);
	glutSolidTorus(0.05, 0.12, NUM_SLICES, NUM_STACKS);
	glPopMatrix();

	/* the shoulders? */
	glTranslatef(0, 0, 0.2);
	glScalef(1,1,1.5);
	glutSolidSphere(0.1, NUM_SLICES, NUM_STACKS);	
}

static void draw_queen(void)
{
	GLUquadricObj *q;
	glScalef(1,1,1.15);

	/* the base */
	glTranslatef(0, 0, 0.07);
	glutSolidTorus(0.06, 0.23, NUM_SLICES, NUM_STACKS);

	/* the torso */
	q = gluNewQuadric();
	gluCylinder(q, 0.25, 0.15, 1.0, NUM_SLICES, NUM_STACKS);
	glTranslatef(0,0, 0.9);

	/* the ruff */
	glutSolidTorus(0.08, 0.16, NUM_SLICES, NUM_STACKS);
	glTranslatef(0,0, 0.15);

	/* the head */
	glutSolidSphere(0.15, NUM_SLICES, NUM_STACKS);

	/* the crown */
	glTranslatef(0,0,0.08);
	glutSolidTorus(0.03, 0.13, NUM_SLICES, NUM_SLICES);

	/* and a knob on top */
	glTranslatef(0,0, 0.07);
	glutSolidSphere(0.07, NUM_SLICES/2, NUM_STACKS/2);
}

static void draw_king(void)
{
	GLUquadricObj *q;
	q = gluNewQuadric();

	/* the base */
	drawBox(-0.27, 0.27, -0.27, 0.27, 0, 0.1);
	glTranslatef(0,0,0.1);

	/* torso and shoulders */
	gluCylinder(q, 0.25, 0.15, 1.0, NUM_SLICES, NUM_STACKS);
	glTranslatef(0,0,1);
	glutSolidTorus(0.07, 0.14, NUM_SLICES, NUM_STACKS);

	/* the cross */
	drawBox(-0.075, 0.075, -0.075, 0.075, 0, 0.4);
	glTranslatef(0,0,0.2);

	drawBox(-0.2, 0.2, -0.075, 0.075, -0.07, 0.07);
}


static void draw_knight(void)
{
	GLUquadricObj *q;
	q = gluNewQuadric();


	glRotatef(-90,0,0,1);

	/* base */
	glTranslatef(0,0,0.05);
	glutSolidTorus(0.1,0.2,NUM_SLICES,NUM_STACKS);
	glTranslatef(0,0,0.1);
	glutSolidTorus(0.1,0.2,NUM_SLICES,NUM_STACKS);
	glTranslatef(0,0,0.05);

	/* torso */
	glRotatef(10, 0, 1, 0);
	gluCylinder(q, 0.2, 0.15, 0.7, NUM_SLICES, NUM_STACKS);
	glTranslatef(0,0, 0.65);

	/* head */
	glutSolidSphere(0.16, NUM_SLICES, NUM_STACKS);

	/* ears */
	glPushMatrix();
	glRotatef(10, 0, 1, 0);
	glRotatef(25, 1, 0, 0);
	glTranslatef(0.05,0,0.13);
	gluCylinder(q, 0.08, 0.0, 0.15, NUM_SLICES/2, NUM_STACKS/2);
	glPopMatrix();

	glPushMatrix();
	glRotatef(10, 0, 1, 0);
	glRotatef(-25, 1, 0, 0);
	glTranslatef(0.05,0,0.13);
	gluCylinder(q, 0.08, 0, 0.15, NUM_SLICES/2, NUM_STACKS/2);
	glPopMatrix();	

	/* nose */
	glRotatef(-110, 0, 1, 0);
	gluCylinder(q, 0.17, 0.1, 0.3, NUM_SLICES, NUM_STACKS);
	glTranslatef(0,0, 0.3);
	glutSolidSphere(0.1, NUM_SLICES, NUM_STACKS);
}


/* its cute to make the bishops and knights face the opponents king */
static void face_king(int p, int x, int y)
{
	Square kpos = PIECES(&state->position, -p)[IKING].pos;
	double dx, dy, theta;

	dx = XPOS(kpos) - x;
	dy = YPOS(kpos) - y;

	theta = 360*atan(dx/(dy+0.01))/(2*M_PI);

	if (dy < 0) theta += 180;

	glRotatef(-theta,0,0,1);
}

static void draw_piece(int p, int x, int y)
{
	glPushMatrix();
	glTranslatef(0.5, 0.5, BOARDGAP);
	if (p > 0) {
		glColor3fv(a_color);
	} else {
		glColor3fv(b_color);
	}

	switch (abs(p)) {
	case PAWN:
		draw_pawn();
		break;
	case KNIGHT:
		face_king(p, x, y);
		draw_knight();
		break;
	case BISHOP:
		face_king(p, x, y);
		draw_bishop();
		break;
	case ROOK:
		draw_rook();
		break;
	case QUEEN:
		draw_queen();
		break;
	case KING:
		draw_king();
		break;
	}

	glPopMatrix();
}


static void draw_pieces(void)
{
	int i,j;
	int p;

	for (i=0;i<8;i++)
		for (j=0;j<8;j++) {
			if (!(p=BOARD(i,j))) continue;
			glPushMatrix();
			glTranslatef(i,j,0);
			draw_piece(p, i, j);
			glPopMatrix();
		}
}

/* this draws the board itself - must be called with lighting 
   off (otherwise we'd need to give the squares some depth) */
static void draw_board(void)
{
	int i, j;
	
	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_LIGHTING);
	glDisable(GL_DITHER);
	for (i=0;i<8;i++)
		for (j=0;j<8;j++) {
			if (!((i+j)&1)) continue;

			glPushMatrix();
			glTranslatef(i, j, 0);
			glColor3fv(square_color_1);
			glRectf(0,0,1,1);
			glPopMatrix();
		}
	glEnable(GL_DITHER);
	glPopAttrib();
}

static void setup_lighting(void)
{
	GLfloat light_position[] = {2, -1, 20.0, 0.0};

	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);

	glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);
}

static void position_board(void)
{
	GLfloat m[4][4];

	glScalef(state->scale, state->scale, state->scale);

	setup_lighting();

	gluPerspective(26, 1, 10, 100);

	glMatrixMode(GL_MODELVIEW);

	glTranslatef(0, 0, -20);

	build_rotmatrix(m, state->curquat);
	glMultMatrixf(&m[0][0]);

	if (state->flip_view) {
		/* its easier to flip the view than to use the trackball */
		glRotatef(180, 0, 0, 1);
	}

	glTranslatef(-4, -4, 0);
}

/* this starts us at a reasonable viewing angle */
static void reset_view(void)
{
	state->scale = 1;
	state->curquat[0] = 0.28;
	state->curquat[1] = 0;
	state->curquat[2] = 0;
	state->curquat[3] = 0.90;
}


void draw_all(void)
{
	need_redraw = 0;
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	position_board();
	draw_pieces();
	draw_board();
        glPopMatrix();

	glutSwapBuffers();
}


/* check if mouse click at (px,py) is in a square, returning the
   square in (*rx,*ry) if it is */
static int in_square(int px,int py, Square *p)
{
	int x, y;
	int ret;
	GLint vp[4];
	int squares_only=1;
	float buffer[1000*4*sizeof(float)];

	glGetIntegerv(GL_VIEWPORT, vp);

        glPushMatrix();

again:

	for (x=0;x<8;x++)
		for (y=0;y<8;y++) {
			glFeedbackBuffer(1000, GL_2D, buffer);
			glRenderMode(GL_FEEDBACK);
			glInitNames();
			glPushName(~0);
			glPushMatrix();
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			gluPickMatrix(px, vp[3] - py, 1, 1, vp);
			position_board();
			glTranslatef(x, y, 0);
			if (!squares_only && BOARD(x,y)) {
				draw_piece(BOARD(x,y), x, y);
			} else {
				glRectf(0, 0, 1, 1);
			}
			glPopMatrix();
				
			ret = glRenderMode(GL_RENDER);
				
			if (ret != 0) {
				(*p) = POSN(x,y);
				glPopMatrix();
				return 1;
			}
		}

	if (!squares_only) {
		squares_only = 1;
		goto again;
	}

	glPopMatrix();

	return 0;
}


static void mouse_func(int button, int bstate, int x, int y)
{
	static Square from, to;
	static int do_movement;

	if (bstate == GLUT_DOWN && button == GLUT_MIDDLE_BUTTON) {
		downx = x;
		downy = y;		
	}

	if (bstate == GLUT_DOWN && button == GLUT_LEFT_BUTTON) {
		mouse_moved = 0;
		downx = x;
		downy = y;
		if(glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
			do_scaling = 1;
		} else if (in_square(x,y,&from) && BOARD1(from)) {
			do_movement = 1;
		} else {
			do_motion = 1;
		}
	}

	if (bstate == GLUT_UP && button == GLUT_LEFT_BUTTON) {
		do_motion = 0;
		do_scaling = 0;
		if (do_movement && in_square(x,y,&to) && to != from &&
		    next_to_play(&state->position) != state->computer) {
			Move move;
			move.from = from; 
			move.to = to;
			prog_tell_move(&state->position,&move);
			player_moved(from, to);
		}
		do_movement = 0;
	}
}

static void motion_func(int x, int y)
{
	float lastquat[4];
	GLint vp[4];
	
	if (x == downx && y == downy) return;

	mouse_moved = 1;

	if (!do_motion && !do_scaling) return;

	glGetIntegerv(GL_VIEWPORT, vp);

	if (do_motion) {
		trackball(lastquat,
			  (2.0*downx - vp[2]) / vp[2],
			  (vp[3] - 2.0*downy) / vp[3],
			  (2.0*x - vp[2]) / vp[2],
			  (vp[3] - 2.0*y) / vp[3]
			  );

		add_quats(lastquat, state->curquat, state->curquat);
	}

	if (do_scaling) {
		state->scale *= (1.0 + (((float) (downy - y)) / vp[3]));
	}

	downx = x;
	downy = y;
	redraw();
}

struct timeval tp1,tp2;

static void start_timer()
{
  gettimeofday(&tp1,NULL);
}

static double end_timer()
{
  gettimeofday(&tp2,NULL);
  return((tp2.tv_sec - tp1.tv_sec) + 
	 (tp2.tv_usec - tp1.tv_usec)*1.0e-6);
}

static void speed_test(void)
{
	float lastquat[4];
	GLint vp[4];
	int counter = 0;
	float x1 = 190, x2 = 200;
	float y1 = 200, y2 = 200;

	start_timer();


	while (end_timer() < 5) {
		glGetIntegerv(GL_VIEWPORT, vp);

		trackball(lastquat,
			  (2.0*x1 - vp[2]) / vp[2],
			  (vp[3] - 2.0*y1) / vp[3],
			  (2.0*x2 - vp[2]) / vp[2],
			  (vp[3] - 2.0*y2) / vp[3]
			  );

		add_quats(lastquat, state->curquat, state->curquat);
		draw_all();
		counter++;
	}
	printf("%4.2f fps\n", counter / end_timer());
}


static void edit_menu(int v)
{
	Square sq;

	if (!in_square(downx, downy, &sq))
		return;

	switch (v) {
	case EDIT_NONE:
		lprintf(0,"remove at %d %d\n", downx, downy);
		state->position.board[sq] = 0;
		break;
	case EDIT_RECREATE:
		lprintf(0,"recreate\n");
		create_pboard(&state->position);
		break;

	case EDIT_CLEAR:
		memset(state->position.board, 0, 
		       sizeof(state->position.board));
		break;

	case EDIT_WHITES_MOVE:
		state->position.move_num &= ~1;
		break;

	case EDIT_BLACKS_MOVE:
		state->position.move_num |= 1;
		break;

	case EDIT_BPAWN:
	case EDIT_BKNIGHT:
	case EDIT_BBISHOP:
	case EDIT_BROOK:
	case EDIT_BQUEEN:
	case EDIT_BKING:
		lprintf(0,"set black %d at %d %d\n",
			v - EDIT_BPAWN, downx, downy);
		
		state->position.board[sq] = -(PAWN + (v - EDIT_BPAWN));
		break;

	case EDIT_WPAWN:
	case EDIT_WKNIGHT:
	case EDIT_WBISHOP:
	case EDIT_WROOK:
	case EDIT_WQUEEN:
	case EDIT_WKING:
		lprintf(0,"set black %d at %d %d\n",
			v - EDIT_WPAWN, downx, downy);
		
		state->position.board[sq] = PAWN + (v - EDIT_WPAWN);
		break;
	}

	redraw();
}


static void main_menu(int v)
{
	switch (v) {
	case MENU_EVAL:
		eval_debug(&state->position);
		break;

	case MENU_SAVE:
		save_game("knightcap.save");
		break;

	case MENU_RESTORE:
		restore_game("knightcap.save");
		break;

	case MENU_DEMO:
		demo_mode = !demo_mode;
		if (!demo_mode) 
			state->stop_search = 1;
		if (demo_mode) {
			glutChangeToMenuEntry(menu_demo_mode, 
					    "disable demo mode", MENU_DEMO);
		} else {
			glutChangeToMenuEntry(menu_demo_mode, 
					    "demo mode", MENU_DEMO);
		}
		break;

	case MENU_QUIT:
		state->stop_search = 1;
		state->quit = 1;
		prog_exit();
		exit(0);
		break;

	case MENU_RESET:
		reset_board();
		break;

	case MENU_RESET_VIEW:
		reset_view();
		break;

	case MENU_FLIP:
		state->flip_view = !state->flip_view;
		break;

	case MENU_SPEED:
		speed_test();
		break;
	}

	redraw();
}


static void computer_menu(int v)
{
	if (v == 2) {
		state->always_think = !state->always_think;
		if (state->always_think) {
			glutChangeToMenuEntry(menu_always_think, 
					    "disable always think",v);
		} else {
			glutChangeToMenuEntry(menu_always_think, 
					    "always think", v);
		}
		return;
	}
	if (v == 3) {
		state->ics_robot = !state->ics_robot;
		if (state->ics_robot) {
			glutChangeToMenuEntry(menu_ics_robot, 
					    "disable ICS robot",v);
		} else {
			glutChangeToMenuEntry(menu_ics_robot, 
					    "enable ICS robot", v);
		}
		return;
	}
	state->stop_search = 1;
	state->computer = v;
}

static void move_time_menu(int v)
{
	state->move_time = v;
}

static void create_menus(void)
{
	int comp, undo, redo, move;

	comp = glutCreateMenu(computer_menu);
	glutAddMenuEntry("computer plays white", PIECE_WHITE);
	glutAddMenuEntry("computer plays black", PIECE_BLACK);
	glutAddMenuEntry("disable computer", 0);
	menu_always_think = 4;
	glutAddMenuEntry("always think", 2);
	menu_ics_robot = 5;
	glutAddMenuEntry("enable ICS robot", 3);

	undo = glutCreateMenu(undo_menu);
	glutAddMenuEntry("1 move", 1);
	glutAddMenuEntry("2 moves", 2);
	glutAddMenuEntry("4 moves", 4);
	glutAddMenuEntry("8 moves", 8);

	redo = glutCreateMenu(undo_menu);
	glutAddMenuEntry("1 move", -1);
	glutAddMenuEntry("2 moves",-2);
	glutAddMenuEntry("4 moves",-4);
	glutAddMenuEntry("8 moves",-8);

	move = glutCreateMenu(move_time_menu);
	glutAddMenuEntry("2  seconds", 2);
	glutAddMenuEntry("5  seconds", 5);
	glutAddMenuEntry("9  seconds", 9);
	glutAddMenuEntry("10 seconds", 10);
	glutAddMenuEntry("30 seconds", 30);
	glutAddMenuEntry("60 seconds", 60);
	glutAddMenuEntry("5  minutes", 300);

	glutCreateMenu(main_menu);
	glutAddMenuEntry("reset game", MENU_RESET);
	glutAddMenuEntry("reset view", MENU_RESET_VIEW);
	glutAddMenuEntry("flip view", MENU_FLIP);
	menu_demo_mode = 4;
	glutAddMenuEntry("demo mode", MENU_DEMO);
	glutAddMenuEntry("eval debug", MENU_EVAL);
	glutAddSubMenu("computer",comp);
	glutAddSubMenu("undo",undo);
	glutAddSubMenu("redo",redo);
	glutAddSubMenu("move time",move);
	glutAddMenuEntry("save game", MENU_SAVE);
	glutAddMenuEntry("restore game", MENU_RESTORE);
	glutAddMenuEntry("quit", MENU_QUIT);
	glutAddMenuEntry("speed test", MENU_SPEED);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	glutCreateMenu(edit_menu);
	glutAddMenuEntry("delete", EDIT_NONE);
	glutAddMenuEntry("recreate", EDIT_RECREATE);
	glutAddMenuEntry("clear board", EDIT_CLEAR);
	glutAddMenuEntry("whites move", EDIT_WHITES_MOVE);
	glutAddMenuEntry("blacks move", EDIT_BLACKS_MOVE);
	glutAddMenuEntry("black pawn", EDIT_BPAWN);
	glutAddMenuEntry("black knight", EDIT_BKNIGHT);
	glutAddMenuEntry("black bishop", EDIT_BBISHOP);
	glutAddMenuEntry("black rook", EDIT_BROOK);
	glutAddMenuEntry("black queen", EDIT_BQUEEN);
	glutAddMenuEntry("black king", EDIT_BKING);
	glutAddMenuEntry("white pawn", EDIT_WPAWN);
	glutAddMenuEntry("white knight", EDIT_WKNIGHT);
	glutAddMenuEntry("white bishop", EDIT_WBISHOP);
	glutAddMenuEntry("white rook", EDIT_WROOK);
	glutAddMenuEntry("white queen", EDIT_WQUEEN);
	glutAddMenuEntry("white king", EDIT_WKING);
	glutAttachMenu(GLUT_MIDDLE_BUTTON);
}

void start_display(int argc,char *argv[])
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);

	reset_view();

	glutCreateWindow("KnightCap");
	glutReshapeWindow(window_size, window_size);
	glutDisplayFunc(draw_all);
	glutMouseFunc(mouse_func);
	glutMotionFunc(motion_func);
	glutIdleFunc(idle_func);
	
	/* set other relevant state information */
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LINE_SMOOTH);
	
	set_cursor(GLUT_CURSOR_CROSSHAIR);
	create_menus();

	glutMainLoop();
}

#else

void redraw(void)
{
	
}

void draw_all(void)
{
	need_redraw = 0;
	print_board(state->position.board);
}
#endif
