#include <unistd.h>
#include <pthread.h>
#include "libmemh.h"

#define MEMH_TEST_SIZE			(5)
#define MEMH_TEST_BLOCK_SIZE	(50000)
#define MEMH_DELAY_UNIT			(1)

#ifdef MEMH_ALLOW_THREAD_TO_PRINT_STATS
#define MEMH_PRINT_STATS_CYCLE	(MEMH_DELAY_UNIT * 5)
/** Separate thread to read the memory stats evrey MEMH_PRINT_STATS_CYCLE seconds. */
void test_print_mem_stats(void* argp) {
	int* thID = (int*) argp;
	fprintf(stderr, "Scanning Thread ID: %d\n", *thID);
	print_dyn_mem_stats();
	sleep(MEMH_PRINT_STATS_CYCLE);
}
#endif

/**
 * Testing malloc()/free() wrapper main function.
 */
int main(int argc, char** argv) {
	char** ary = (char**)malloc(sizeof(char*) * MEMH_TEST_SIZE);
	int size = MEMH_TEST_SIZE;
#ifdef MEMH_ALLOW_THREAD_TO_PRINT_STATS
	pthread_t thID1;
	pthread_create(&thID1, NULL, test_print_mem_stats, (void*)&thID1);
#endif
	if (argc > 1) {
		size = atoi(argv[1]);
		/** Test up to 100 mem blocks. */
		if ((size > 0) || (size < 100)) {
			fprintf(stderr, "Using %d blocks\n", size);
		} 	else {
			fprintf(stderr, "\n\nError: enterd non-valid size: %d\n", size);
			exit(1);
		}
	}

	fprintf(stderr, "Testing Michel Hanna dynamic memory allocation wrapper\n");
	fprintf(stderr, "\t\tUsage: <prog_name> <num_mem_blocks>,  where num_mem_blocks:[0-99]\n");
	for (int i = 0; i < size; i++) {
		fprintf(stderr, "Allocating one block %d\n", i);
		char* ptr = (char*)malloc(MEMH_TEST_BLOCK_SIZE);
		if (ptr == NULL) {
			fprintf(stderr, "\n\nError: Cannot allocate Memory\n");
			exit(1);
		}
		ary[i] = ptr;
		sleep(MEMH_DELAY_UNIT);
	}
#ifndef MEMH_ALLOW_THREAD_TO_PRINT_STATS
	print_dyn_mem_stats();
	sleep(MEMH_DELAY_UNIT * 2);
#endif
	for (int i = 0; i < size; i++) {
		fprintf(stderr, "Freeing one block %d\n", i);
		char* ptr = ary[i];
		free(ptr);
		usleep(MEMH_DELAY_UNIT);
	}

	sleep(MEMH_DELAY_UNIT * 2);
	free(ary);
#ifndef MEMH_ALLOW_THREAD_TO_PRINT_STATS
	/** Final memory stasts ...*/
	print_dyn_mem_stats();
#else
	pthread_join(thID1, NULL);
#endif

	return 0;
}