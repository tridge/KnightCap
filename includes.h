#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <stdarg.h>

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

#ifdef APLINUX
#include <cap2.c.h>
#endif

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#include "local.h"
