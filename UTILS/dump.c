#include <stdio.h>
#include <fcntl.h>
#include "knightcap.h"
#include "names.h"

static void p_coeff_vector(struct coefficient_name *cn, FILE *fd)
{
	int x;
	fprintf(fd,"/* %s */\n", cn->name);
	for (x=0; x<(cn+1)->index - cn->index; x++) {
		fprintf(fd,"%7d,", coefficients[cn->index + x]);
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

void dump_coeffs(char *fname) 
{
	struct coefficient_name *cn; 
	FILE *fd = fopen(fname, "w");

	if (fd == NULL) {
		perror(fname);
		return 0;
	}

	fprintf(fd, "etype orig_coefficients[] = {\n");
	cn = &coefficient_names[0];
	while (cn->name) {
		int n = cn[1].index - cn[0].index;
		if (n == 1) {
			printf("/* %s */ %d,\n", cn[0].name, coefficients[cn[0].index]);
		} else if (n == 64) {
			p_coeff_board(cn,fd);
		} else if (n == 32) {
			p_coeff_half_board(cn,fd);
		} else {
			p_coeff_vector(cn,fd);			
		}
		cn++;
	}
	
	fprintf(fd, "};\n");
	fclose(fd);
}




