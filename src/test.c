/*
 * ============================================================================
 *
 *        Authors:  Prashant Pandey <ppandey@cs.stonybrook.edu>
 *                  Rob Johnson <robj@vmware.com>   
 *
 * ============================================================================
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <openssl/rand.h>

#include "include/gqf.h"
#include "include/gqf_int.h"
#include "include/gqf_file.h"

int find(uint64_t* array, int len, uint64_t item) {
	int i;
	for (i = 0; i < len; i++)
		if (array[i] == item) return 1;
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Please specify the log of the number of slots and the number of remainder bits in the CQF.\n");
		exit(1);
	}
	QF qf;
	uint64_t qbits = atoi(argv[1]);
	uint64_t rbits = atoi(argv[2]);
	uint64_t nhashbits = qbits + rbits;
	uint64_t nslots = (1ULL << qbits);
	uint64_t nvals = 95*nslots/100;
	uint64_t key_count = 4;
	uint64_t *vals;

	/* Initialise the CQF */
	/*if (!qf_malloc(&qf, nslots, nhashbits, 0, QF_HASH_INVERTIBLE, 0)) {*/
	/*fprintf(stderr, "Can't allocate CQF.\n");*/
	/*abort();*/
	/*}*/
	if (!qf_initfile(&qf, nslots, nhashbits, 0, QF_HASH_INVERTIBLE, 0,
									 "mycqf.file")) {
		fprintf(stderr, "Can't allocate CQF.\n");
		abort();
	}

	qf_set_auto_resize(&qf, true);
	
	int universe = 100000;
	int num_inserts = 100;
	int num_queries = 10000;
	int count_fp = 0;
	
	//uint64_t *nodes = malloc(sizeof(node) * num_inserts);
	//uint64_t *tree = nodes[0]; // simple reverse map for testing - index equals hash
	uint64_t* values = malloc(sizeof(uint64_t) * num_inserts);
	uint64_t *ret_index = malloc(sizeof(uint64_t));
	uint64_t *ret_hash = malloc(sizeof(uint64_t));
	
	clock_t start_time, end_time;
	start_time = clock();
	
	/*printf("insert returned %lu\n", qf_insert(&qf, 1, 0, 1, QF_NO_LOCK | QF_KEY_IS_HASH));
	printf("insert returned %lu\n", qf_insert(&qf, 2, 0, 1, QF_NO_LOCK | QF_KEY_IS_HASH));
	
	printf("query returned %lu\n", qf_query(&qf, 1, 0, ret_index, ret_hash, QF_KEY_IS_HASH));
	printf("query returned %lu\n", qf_query(&qf, 2, 0, ret_index, ret_hash, QF_KEY_IS_HASH));
	printf("query returned %lu\n", qf_query(&qf, 3, 0, ret_index, ret_hash, QF_KEY_IS_HASH));
	
	printf("adapt returned %d\n", qf_adapt(&qf, 1, 1, (1ULL << 32) | 1ULL, QF_KEY_IS_HASH));
	printf("adapt returned %d\n", qf_adapt(&qf, 0, 2, (1ULL << 32) | 2ULL, QF_KEY_IS_HASH));
	
	printf("query returned %lu\n", qf_query(&qf, 1, 0, ret_index, ret_hash, QF_KEY_IS_HASH));
	printf("query returned %lu\n", qf_query(&qf, (1ULL << 32) | 1ULL, 0, ret_index, ret_hash, QF_KEY_IS_HASH));
	printf("query returned %lu\n", qf_query(&qf, 2, 0, ret_index, ret_hash, QF_KEY_IS_HASH));
	printf("query returned %lu\n", qf_query(&qf, (1ULL << 32) | 2ULL, 0, ret_index, ret_hash, QF_KEY_IS_HASH));*/
	
	srand(time(NULL));
	
	uint64_t i, j;
	for (i = 0; i < num_inserts;) {
		j = (uint64_t)(rand() % universe);
		//printf("inserting %d\n", j);
		if (!find(values, num_inserts, j)) {
			values[i++] = j;
			qf_insert(&qf, j, 0, 1, QF_NO_LOCK | QF_KEY_IS_HASH);
		}
	}
	printf("finished insertions\n");
	
	int r;
	for (i = 0; i < num_queries; i++) {
		j = (uint64_t)(rand() % universe);
		if (qf_query(&qf, j, 0, ret_index, ret_hash, QF_KEY_IS_HASH) && !find(values, num_inserts, j)) {
			if (!find(values, num_inserts, j)) {
				count_fp++;
				qf_adapt(&qf, *ret_index, *ret_hash, j, QF_KEY_IS_HASH);
			}
		}
	}
	
	end_time = clock();
	printf("completed in time %ld\n", end_time - start_time);
	printf("performed %d queries from a universe of size %d on a filter with %d items\n", num_queries, universe, num_inserts);
	printf("encountered %d false positives\n", count_fp);
	
	free(ret_index);
	free(ret_hash);
	free(values);
}

