#include <stdio.h>
#include <fcntl.h>

typedef int etype;
#include "eval.h"
#include "names.h"

etype coefficients[__TOTAL_COEFFS__];


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

main() 
{
	struct coefficient_name *cn; 
	int fd = open("coeffs.dat",O_RDWR); 

	if (fd == -1) {
		perror("coeffs.dat");
		return 0;
	}

        read(fd,coefficients,__TOTAL_COEFFS__*sizeof(coefficients[0]));

	fprintf(stdout, "etype orig_coefficients[] = {\n");
	cn = &coefficient_names[0];
	while (cn->name) {
		int n = cn[1].index - cn[0].index;
		if (n == 1) {
			printf("/* %s */ %d,\n", cn[0].name, coefficients[cn[0].index]);
		} else if (n == 64) {
			p_coeff_board(cn,stdout);
		} else if (n == 32) {
			p_coeff_half_board(cn,stdout);
		} else {
			p_coeff_vector(cn,stdout);			
		}
		cn++;
	}

	fprintf(stdout, "};\n");
	
}
