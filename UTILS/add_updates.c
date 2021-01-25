#include <stdio.h>
#include <fcntl.h>
#include <math.h>

#define EVAL_SCALE (atanh(0.25)/10000) /* tanh(1 pawn up) = 0.25 */
#define TD_ALPHA (0.1/(EVAL_SCALE))

typedef enum {OPENING=0, MIDDLE, ENDING, MATING} GameStage;
char *stage_name[] = {"OPENING", "MIDDLE", "ENDING", "MATING"};

#define __COEFFS_PER_STAGE__ (__TOTAL_COEFFS__/(MATING+1))

typedef int etype;

#include "eval.h"
#include "names.h"

double uf[__TOTAL_COEFFS__];
etype new_coefficients[__TOTAL_COEFFS__];
etype *coefficients;

static void p_coeff_vector(struct coefficient_name *cn, FILE *fd)
{
	int x;
	fprintf(fd,"/* %s */\n", cn->name);
	for (x=0; x<(cn+1)->index - cn->index; x++) {
		fprintf(fd,"%7d,", coefficients[cn->index + x]);
	}
	fprintf(fd,"\n");
}

static void p_coeff_array(struct coefficient_name *cn, FILE *fd)
{
	int x;
	fprintf(fd,"/* %s */\n", cn->name);
	for (x=0; x<(cn+1)->index - cn->index; x++) {
		fprintf(fd,"%7d,", coefficients[cn->index + x]);
		if ((x+1)%10 == 0) {
			fprintf(fd, "\n");
		}
	}
	fprintf(fd,"\n");
}

static void p_coeff_board(struct coefficient_name *cn, FILE *fd)
{
	int x, y;
	fprintf(fd,"/* %s */\n", cn->name);
	for (y=0; y<8; y++) {
		for (x=0; x<8; x++) {
			fprintf(fd,"%7d,", coefficients[cn->index + x + y*8]);
		}
		fprintf(fd,"\n");
	}
}

static void p_coeff_half_board(struct coefficient_name *cn, FILE *fd)
{
	int x, y;
	fprintf(fd,"/* %s */\n", cn->name);
	for (y=0; y<8; y++) {
		for (x=0; x<4; x++) {
			fprintf(fd,"%7d,", coefficients[cn->index + x + y*4]);
		}
		fprintf(fd,"\n");
	}
}

main(int argc, char *argv[]) 
{
	struct coefficient_name *cn; 
	int fdu, fdc;
	int i;
	double norm;

	fdc = open("coeffs.dat", O_RDONLY);
	fdu = open("update.dat", O_RDONLY);
 
	if (fdc == -1) {
		perror("coeffs.dat");
		return 0;
	}

	if (fdu == -1) {
		perror("update.dat");
		return 0;
	}

        read(fdc, new_coefficients, __TOTAL_COEFFS__*sizeof(new_coefficients[0]));
        read(fdu, uf, __TOTAL_COEFFS__*sizeof(uf[0]));
	norm = 0.0;
	for (i=0; i<__TOTAL_COEFFS__; i++) {
		new_coefficients[i] += TD_ALPHA*uf[i];
		norm += fabs(TD_ALPHA*uf[i]);
	}
	fprintf(stderr, "%lg\n", norm);

	fprintf(stdout, "etype orig_coefficients[] = {\n");

	cn = &coefficient_names[0];
	for (i=OPENING; i<=MATING; i++) {
		fprintf(stdout, "\n/* %%%s%% */\n", stage_name[i]);
		cn = &coefficient_names[0];
		coefficients = 	new_coefficients + i*__COEFFS_PER_STAGE__;
		while (cn->name) {
			int n = cn[1].index - cn[0].index;
			if (n == 1) {
				fprintf(stdout, "/* %s */ %d,\n", cn[0].name, 
					coefficients[cn[0].index]);
			} else if (n == 64) {
				p_coeff_board(cn,stdout);
			} else if (n == 32) {
				p_coeff_half_board(cn,stdout);
			} else if (n % 10 == 0) {
				p_coeff_array(cn,stdout);
			} else {
				p_coeff_vector(cn,stdout);
			}
			cn++;
		}
	}

	fprintf(stdout, "};\n");
	
}
