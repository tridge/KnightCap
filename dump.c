#include "includes.h"
#include "knightcap.h"
#include "names.h"

static void p_coeff_vector(struct coefficient_name *cn, FILE *fd,float scale)
{
	int x;
	fprintf(fd,"/* %s */\n", cn->name);
	for (x=0; x<(cn+1)->index - cn->index; x++) {
		fprintf(fd,"%7d,", 
			(int)rint(coefficients[cn->index + x]*scale));
	}
	fprintf(fd,"\n");
}


static void p_coeff_board(struct coefficient_name *cn, FILE *fd,float scale)
{
	int x, y;
	fprintf(fd,"/* %s */\n", cn->name);
	for (y=0; y<8; y++) {
		for (x=0; x<8; x++) {
			fprintf(fd,"%7d,", 
				(int)rint(coefficients[cn->index + x + y*8]*scale));
		}
		fprintf(fd,"\n");
	}
}

static void p_coeff_half_board(struct coefficient_name *cn, FILE *fd,float scale)
{
	int x, y;
	fprintf(fd,"/* %s */\n", cn->name);
	for (y=0; y<8; y++) {
		for (x=0; x<4; x++) {
			fprintf(fd,"%7d,", 
				(int)rint(coefficients[cn->index + x + y*4]*scale));
		}
		fprintf(fd,"\n");
	}
}

void dump_coeffs(char *fname, float scale) 
{
	struct coefficient_name *cn; 
	FILE *fd = fopen(fname, "w");

	if (fd == NULL) {
		perror(fname);
		return;
	}

	fprintf(fd, "CONST_EVAL etype coefficients[] = {\n");
	cn = &coefficient_names[0];
	while (cn->name) {
		int n = cn[1].index - cn[0].index;
		if (n == 1) {
			fprintf(fd, "/* %s */ %d,\n", 
				cn[0].name, 
				(int)rint(coefficients[cn[0].index]*scale));
		} else if (n == 64) {
			p_coeff_board(cn,fd,scale);
		} else if (n == 32) {
			p_coeff_half_board(cn,fd,scale);
		} else {
			p_coeff_vector(cn,fd,scale);			
		}
		cn++;
	}
	
	fprintf(fd, "};\n");
	fclose(fd);
}




