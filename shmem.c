#include "includes.h"

#if USE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>

/* return a pointer to a anonymous shared memory segment of size "size"
   which will persist across fork() but will disappear when all processes
   exit 

   The memory is not zeroed 

   This function uses system5 shared memory. It takes advantage of a property
   that the memory is not destroyed if it is attached when the id is removed
   */
void *shm_allocate(int size)
{
	int shmid;
	void *ret;
	struct shmid_ds shm_ds;
	int page_size = getpagesize();

	size = (size + page_size) & ~(page_size-1);

	shmid = shmget(IPC_PRIVATE, size, SHM_R | SHM_W);
	if (shmid == -1) {
		printf("can't get shared memory\n");
		exit(1);
	}
	ret = (void *)shmat(shmid, 0, 0);
	if (!ret || ret == (void *)-1) {
		printf("can't attach to shared memory\n");
		return NULL;
	}

	if (shmctl(shmid, IPC_STAT, &shm_ds) != 0) {
		printf("ERROR: can't IPC_STAT\n");
	}
	printf("nattch=%d\n", shm_ds.shm_nattch);

	/* the following releases the ipc, but note that this process
	   and all its children will still have access to the memory, its
	   just that the shmid is no longer valid for other shm calls. This
	   means we don't leave behind lots of shm segments after we exit 

	   See Stevens "advanced programming in unix env" for details
	   */
	shmctl(shmid, IPC_RMID, 0);


	
	return ret;
}
#endif



#if USE_DEVZERO
#include <sys/mman.h>
#include <fcntl.h>

/* return a pointer to a /dev/zero shared memory segment of size "size"
   which will persist across fork() but will disappear when all processes
   exit 

   The memory is zeroed automatically

   This relies on /dev/zero being shared mmap capable, which it is
   only under some OSes (Linux 2.1 _not_ included)
 */
void *shm_allocate(int size)
{
	void *ret;
	int fd;
	int page_size = getpagesize();

	size = (size + page_size) & ~(page_size-1);

	fd = open("/dev/zero", O_RDWR);
	if (fd == -1) {
		return NULL;
	}
	ret = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE,
		   fd, 0);

	/* note that we don't need to keep the file open */
	close(fd);
	if (ret == (void *)-1) return NULL;
	return ret;
}
#endif


#if USE_MAPANON
#include <sys/mman.h>

/* return a pointer to a /dev/zero shared memory segment of size "size"
   which will persist across fork() but will disappear when all processes
   exit 

   The memory is zeroed automatically

   This relies on anonyous shared mmap, which only some OSes can
   do. Linux 2.1 is _not_ one of them.
   */
void *shm_allocate(int size)
{
	void *ret;
	int page_size = getpagesize();

	size = (size + page_size) & ~(page_size-1);

	ret = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED,
		   -1, 0);
	if (ret == (void *)-1) return NULL;
	return ret;
}
#endif


#if USE_MAPFILE
#include <sys/mman.h>
#include <fcntl.h>

/* return a pointer to a /dev/zero shared memory segment of size "size"
   which will persist across fork() but will disappear when all processes
   exit 

   The memory is zeroed automatically

   This relies only on shared mmap, which almost any mmap capable OS
   can do. Linux can do this as of version 1.2 (I believe). certainly
   all recent versions can do it.
  */
void *shm_allocate(int size)
{
	void *ret;
	int fd, zero=0;
	char template[20] = "/tmp/shm.XXXXXX";
	int page_size = getpagesize();

	size = (size + page_size) & ~(page_size-1);

	if (!mktemp(template)) return NULL;
	fd = open(template,O_RDWR|O_CREAT|O_EXCL,0600);
	if (fd == -1) {
		return NULL;
	}
	lseek(fd, size, SEEK_SET);
	write(fd, &zero, sizeof(zero));
	ret = mmap(0, size, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED,
		   fd, 0);
	close(fd);
	unlink(template);
	if (ret == (void *)-1) return NULL;
	return ret;
}
#endif


