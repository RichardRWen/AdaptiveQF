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

#include "sglib.h"

int bp2() {
	return 0;
}

int snapshot(QF *qf) {
	FILE *fp = fopen("output.txt", "w");
	if (fp == NULL) return 0;
	char buffer1[128];
	char buffer2[256];
	bzero(buffer1, 128);
	bzero(buffer2, 256);
	int i, j;
	for (i = 0; i * 64 < qf->metadata->xnslots; i++) {
		uint64_t occupied = get_block(qf, i)->occupieds[0];
		for (j = 0; j < 64; j++) {
			buffer1[63 - j] = '0' + occupied % 2;
			occupied >>= 1;
		}
		sprintf(buffer2, "%d\t%s\n", i, buffer1);
		//printf("%s", buffer2);
		fputs(buffer2, fp);
		uint64_t runend = get_block(qf, i)->runends[0];
		for (j = 0; j < 64; j++) {
			buffer1[63 - j] = '0' + runend % 2;
			runend >>= 1;
		}
		sprintf(buffer2, "\t%s\n", buffer1);
		//printf("%s", buffer2);
		fputs(buffer2, fp);
		uint64_t extension = get_block(qf, i)->extensions[0];
		for (j = 0; j < 64; j++) {
			buffer1[63 - j] = '0' + extension % 2;
			extension >>= 1;
		}
		sprintf(buffer2, "\t%s\n", buffer1);
		fputs(buffer2, fp);
	}
	fclose(fp);
	return 1;
}

struct _keyValuePair {
	uint64_t key;
	uint64_t val;
	struct _keyValuePair *left;
	struct _keyValuePair *right;
} typedef keyValuePair;

#define HASH_TABLE_SIZE  1000000

struct _ilist {
	uint64_t val; // the value of the actual item
	uint64_t rem; // the remainder representing the item in the filter including extensions
	int len; // the number of bits used by the remainder
	struct ilist *next;
} typedef ilist;
typedef struct sglib_hashed_ilist_iterator ilist_iter;
ilist *htab[HASH_TABLE_SIZE];

#define ILIST_COMPARATOR(e1, e2)    ((e1->rem + (1 << e1->len)) - (e2->rem + (1 << e2->len)))

unsigned int ilist_hash_function(ilist *e) {
	return e->rem + (1 << e->len);
}

SGLIB_DEFINE_LIST_PROTOTYPES(ilist, ILIST_COMPARATOR, next)
SGLIB_DEFINE_LIST_FUNCTIONS(ilist, ILIST_COMPARATOR, next)
SGLIB_DEFINE_HASHED_CONTAINER_PROTOTYPES(ilist, HASH_TABLE_SIZE, ilist_hash_function)
SGLIB_DEFINE_HASHED_CONTAINER_FUNCTIONS(ilist, HASH_TABLE_SIZE, ilist_hash_function)

int find(uint64_t* array, int len, uint64_t item) {
	uint64_t i;
	for (i = 0; i < len; i++)
		if (array[i] == item) return 1;
	return 0;
}

// returns the number of low order bits on which hash1 and hash2 match
int hashCmp(uint64_t hash1, uint64_t hash2) {
	//printf("hashCmp: %lu, %lu\n", hash1, hash2);
	int i;
	for (i = 0; i < 64; i++) {
		if ((hash1 & 1) != (hash2 & 1)) break;
		hash1 >>= 1;
		hash2 >>= 1;
	}
	return i;
}

keyValuePair *getItem(keyValuePair *root, uint64_t hash) {
	if (root == NULL) return root;
	//printf("getItem: %lu, %lu\n", root->key, hash);
	int cmp = hashCmp(root->key, hash);
	if (cmp == 64) return root;
	keyValuePair *ret = NULL;
	if (((hash >> cmp) & 1) > ((root->key >> cmp) & 1)) {
		if (root->right != NULL) ret = getItem(root->right, hash);
		else return root;
	}
	else {
		if (root->left != NULL) ret = getItem(root->left, hash);
		else return root;
	}
	//printf("%lu, %lu\n", ret->key, hash);
	if (hashCmp(ret->key, hash) > cmp) return ret;
	else return root;
}

keyValuePair *insertItem(keyValuePair* root, keyValuePair *item) {
	if (root == NULL || item == NULL) return item;
	//printf("insItem: %lu, %lu\n", root->key, item->key);
	int cmp = hashCmp(root->key, item->key);
	if (cmp / 128 > 0) {
		if (root->right == NULL) root->right = item;
		else insertItem(root->right, item);
	}
	else {
		if (root->left == NULL) root->left = item;
		else insertItem(root->left, item);
	}
	return root;
}

int matchpart(uint64_t item, uint64_t hash, uint64_t hash_len, uint64_t qbits, uint64_t rbits) {
	if ((item & ((2 << rbits) - 1)) != (hash & ((2 << rbits) - 1))) return 0;
	hash_len -= rbits;
	hash >>= rbits;
	item >>= qbits + rbits;
	while (hash_len > 0) {
		if ((item & ((2 << rbits) - 1)) != (hash & ((2 << rbits) - 1))) return 0;
		hash_len -= rbits;
		hash >>= rbits;
		item >>= rbits;
	}
	return 1;
}

int findpart(uint64_t* array, int len, uint64_t hash, uint64_t hash_len, uint64_t qbits, uint64_t rbits) {
	int i;
	for (i = 0; i < len; i++)
		if (matchpart(array[i], hash, hash_len, qbits, rbits)) return 1;
	return -1;
}

void printbin(uint64_t val) {
	int i;
	for (i = 63; i >= 0; i--) {
		printf("%lu", (val >> i) % 2);
	}
	printf("\n");
}

ilist *find_ilist(uint64_t hash) {
	return NULL;
}

double squeeze = 45;
int main(int argc, char **argv)
{
	if (argc < 7) {
		fprintf(stderr, "Please specify \nthe log of the number of slots in the QF\nthe number of remainder bits in the QF\nthe universe size\nthe number of inserts\nthe number of queries\nthe number of trials\n");
		// eg. ./test 8 7 100000000 20000000 1000000 20
		// ./test 16 7 100000000 20000000 1000000 1 0
		exit(1);
	}
	if (argc >= 8) {
		srand(atoi(argv[7]));
	}
	else {
		srand(time(NULL));
		//srand(3);
	}
	double avgInsTime = 0, avgQryTime = 0, avgFP = 0, avgFill = 0;
	int numtrials = atoi(argv[6]);
	int trials = 0;
	for (; trials < numtrials; trials++) {
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

	qf_set_auto_resize(&qf, false);
	
	int universe = atoi(argv[3]);//1000000;
	int num_inserts = atoi(argv[4]);//100;
	int num_queries = atoi(argv[5]);//100000;
	int count_fp = 0, count_p = 0;
	
	//uint64_t *nodes = malloc(sizeof(node) * num_inserts);
	//uint64_t *tree = nodes[0]; // simple reverse map for testing - index equals hash
	//uint64_t* values = malloc(sizeof(uint64_t) * num_inserts);
	uint64_t *ret_index = malloc(sizeof(uint64_t));
	uint64_t *ret_hash = malloc(sizeof(uint64_t));
	uint64_t *ret_hash_len = malloc(sizeof(uint64_t));
	
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
	
	/*qf_insert_ret(&qf, 1, 0, 1, QF_KEY_IS_HASH, ret_index, ret_hash, ret_hash_len);
	printf("collided query expected, got %d\n", qf_insert_ret(&qf, 1 + qf.metadata->range, 0, 1, QF_KEY_IS_HASH | QF_NO_LOCK, ret_index, ret_hash, ret_hash_len));
	printf("extended length: %d\n", insert_and_extend(&qf, *ret_index, 1 + qf.metadata->range, 0, 1, 1, 0, QF_KEY_IS_HASH | QF_NO_LOCK));*/
	
	/*qf_insert_ret(&qf, 1, 0, 1, QF_KEY_IS_HASH, ret_index, ret_hash, ret_hash_len);
	printf("%d\n", qf_insert_ret(&qf, 1 + (1 << (qbits + rbits)), 0, 1, QF_KEY_IS_HASH, ret_index, ret_hash, ret_hash_len));
	if (insert_and_extend(&qf, *ret_index, 1 + (1 << (qbits + rbits)), 0, 1, 1, 0, ret_hash, ret_hash_len, QF_KEY_IS_HASH | QF_NO_LOCK) > 0) {
		printf("one fingerprint extended to %lu\n", *ret_hash);
		printf("one fingerprint extended to %lu\n", *ret_hash_len);
	}
	printf("%lu\n", find_first_test(&qf, 0)); // tests find_first_empty_slot
	snapshot(&qf);
	abort();*/
	
	if (0) {
		printf("%d\n", QF_COULDNT_LOCK);
		uint64_t q = 0, p = 0, m;
		for (m = 0; m < num_queries; m++) {
			q = (uint64_t)(rand() % universe);
			if (q % (1 << rbits) < (1 << qbits) && q % (1 << rbits) == 0) {
				p++;
			}
		}
		printf("false positive rate: %f\n", (double) p / num_queries);
		abort();
	}
	
	keyValuePair *val_mem = malloc(sizeof(keyValuePair) * fmax(num_inserts, nslots));
	if (val_mem == NULL) {
		printf("Unable to malloc enough space\n");
		abort();
	}
	int val_cnt = 0;
	keyValuePair *values = NULL;
	
	sglib_hashed_ilist_init(htab);
	ilist ii, *nn, *ll;
	ilist_iter it;
	
	if (universe < (double)qf.metadata->range * 0.8) {
		printf("warning: universe may be too small to fill filter to completion\n");
	}
	
	clock_t start_time, end_time;
	start_time = clock();
	
	uint64_t i, j, k = 0, l = 0;
	double z, y, x;
	for (i = 0; i < num_inserts;) {
		j = (uint64_t)(rand() % universe); // pick a random number
		/*z = (double)rand() / (double)RAND_MAX;
		y = (double)rand() / (double)RAND_MAX;
		x = sqrt(-2 * log(z)) * cos(2 * M_PI * y) + squeeze / 2;
		j = (uint64_t) (x * (double)universe / squeeze);*/
		//printf("%f\n", x);
		//printf("inserting %lu\n", j);
		//if (values == NULL || getItem(values, j)->key != j) {
		//ii.i = j & ((1 << (qbits + rbits)) - 1);
		/*if (i == 0) {
			printf("first item inserted: %lu\n", ii.i);
		}*/
		//ii.rem = j;
    		//if (sglib_hashed_ilist_find_member(htab, &ii) == NULL) { // if the number is unique
			int ret = qf_insert_ret(&qf, j, 0, 1, QF_NO_LOCK | QF_KEY_IS_HASH, ret_index, ret_hash, ret_hash_len); // attempt to insert
			if (ret == QF_NO_SPACE) { // if filter is full, stop
				printf("filter is full\n");
				break;
			}
			else if (ret == 0) { // if a matching fingerprint is found, search hash table for original item
				if (0) {
					continue;
				}
				if (0) {
					//printf("wanted to extend, skipping for testing\n");
					k++;
					l += 3;
					i++;
					continue;
				}
				ii.rem = *ret_hash;
				ii.len = *ret_hash_len;
				ilist *item_in_table = sglib_hashed_ilist_find_member(htab, &ii);
				if (item_in_table == NULL) printf("error:\tfilter claimed to have fingerprint %lu but hashtable could not find it\n", ii.rem);
				else if (item_in_table->val == j) printf("log:\trandom insert %lu would have been duplicate, skipping\n", ii.rem);
				else {
					uint64_t new_rem, upd_rem;
					int ext_len = insert_and_extend(&qf, *ret_index, j, 0, 1, item_in_table->val, 0, &new_rem, &upd_rem, QF_KEY_IS_HASH | QF_NO_LOCK);
					if (ext_len == QF_NO_SPACE) {
						printf("filter is full\n");
						break;
					}
					nn = malloc(sizeof(ilist));
					nn->val = item_in_table->val;
					nn->rem = upd_rem;
					nn->len = ext_len;
					sglib_hashed_ilist_delete(htab, item_in_table);
					free(item_in_table);
					sglib_hashed_ilist_add(htab, nn);
					nn = malloc(sizeof(ilist));
					nn->val = j;
					nn->rem = new_rem;
					nn->len = ext_len;
					sglib_hashed_ilist_add(htab, nn);
					//printf("extended to length %d\n", insert_and_extend(&qf, *ret_index, j, 0, 1, findpart(vals, i - 1, *ret_hash, *ret_hash_len, qbits, rbits), 0, QF_KEY_IS_HASH | QF_NO_LOCK));
					k++;
					i++;
				}
			}
			else if (ret == 1) {
				nn = malloc(sizeof(ilist));
				nn->val = j;
				nn->rem = *ret_hash;
				nn->len = *ret_hash_len;
				sglib_hashed_ilist_add(htab, nn);
				/*val_mem[val_cnt].key = val_mem[val_cnt].val = j & BITMASK(rbits);
				val_mem[val_cnt].left = val_mem[val_cnt].right = NULL;
				values = insertItem(values, &(val_mem[val_cnt]));
				val_cnt++;*/
				//l++;
				i++;
			}
			/*else if (ret == -5) {
				continue;
			}*/
			else {
				printf("other error: errno %d\n", ret);
				break;
			}
			//i++;
		//}
	}
	avgFill += (double)i / nslots;
	printf("made %lu inserts\n", i);
	printf("extended %lu times\n", k);
	//printf("%lu\n", qf_get_num_occupied_slots(&qf));
	avgInsTime += (clock() - start_time);
	start_time = clock();
	
	printf("starting %d queries...\n", num_queries);
	int r;
	for (i = 0; i < num_queries; i++) {
		//printf("%lu\n", i);
		j = (uint64_t)(rand() % universe);
		//if (qf_query(&qf, j, 0, ret_index, ret_hash, QF_KEY_IS_HASH) && getItem(values, j)->key != j) {
		//ii.rem = j & ((1 << rbits) - 1);
		if (qf_query(&qf, j, 0, ret_index, ret_hash, ret_hash_len, QF_KEY_IS_HASH)) {
			count_p++;
			ii.rem = *ret_hash;
			ii.len = *ret_hash_len;
			if (sglib_hashed_ilist_find_member(htab, &ii) == NULL) {
				count_fp++;
				if (1) {
					continue;
				}
				/*if (qf_adapt(&qf, *ret_index, *ret_hash, j, QF_KEY_IS_HASH) == -2) { // TODO: when reactivating this, need to add code to modify hash table
					printf("ran out of space to adapt after %lu queries\n", i);
					break;
				}*/
			}
		}
	}
	
	end_time = clock();
	printf("completed in time %ld us\n", end_time - start_time);
	printf("performed %d queries from a universe of size %d on a filter with %d items\n", num_queries, universe, num_inserts);
	printf("false positive rate: %f\n", (double)count_fp / count_p);
	
	free(ret_index);
	free(ret_hash);
	free(values);
	for(ll=sglib_hashed_ilist_it_init(&it,htab); ll!=NULL; ll=sglib_hashed_ilist_it_next(&it)) {
		free(ll);
	}
	avgQryTime += (end_time - start_time);
	avgFP += (double)count_fp / count_p;
	}
	printf("\nperformed %d trials\n", numtrials);
	printf("avg false positive rate: %f\n", avgFP / numtrials);
	printf("avg fill rate: %f\n", avgFill / numtrials);
	printf("avg total insert time: %f\n", avgInsTime / numtrials);
	printf("avg total query time: %f\n", avgQryTime / numtrials);
}

