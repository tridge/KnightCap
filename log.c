#include "includes.h"
#include "knightcap.h"

static FILE *logfile;
static FILE *cfile;

void log_close(void)
{
	fclose(logfile);
	logfile = NULL;
}

int lprintf(int level, char *format_str, ...)
{
	va_list ap;  
	char fmt[4096];
	char *p;

	if (!logfile) {
#if APLINUX
		logfile = fopen("/ddv/data/tridge/knightcap.log","w");
#else
		logfile = fopen("knightcap.log","w");
#endif
		setlinebuf(logfile);
	}

	va_start(ap, format_str);
	strcpy(fmt, format_str);
	if (sizeof(short) == sizeof(etype)) {
		while ((p = strstr(fmt, "%e"))) {
			p[1] = 'd';
		}
	} else {
		while ((p = strstr(fmt, "%f"))) {
			p[1] = 'd';
		}
	}


	if (level == 0) {
		vfprintf(stdout,fmt,ap);
	}

	va_end(ap);
	va_start(ap, format_str);

	if (logfile) {
		vfprintf(logfile,fmt,ap);
	}

	va_end(ap);


	return(0);
}

void lindent(int level, int ply)
{
	while (ply--) 
		lprintf(level, "\t");
}

int cprintf(int level, char *format_str, ...)
{
	va_list ap;  


	if (!cfile) {
		cfile = fopen("wnorm.dat", "w");
		setlinebuf(cfile);
	}

	va_start(ap, format_str);
	
	vfprintf(cfile,format_str,ap);

	va_end(ap);


	return(0);
}
