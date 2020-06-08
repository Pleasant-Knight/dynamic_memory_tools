/**
 * Michel Hanna - (C) 2020.
 * Defining malloc/free/calloc/realloc wrappers for gathering temporal 
 * and spatial stats about allocations.
 *
 * Using "_GNU_SOURCE" for the following reasons:
 * Access many of nonstandard GNU/Linux extension functions.
 * Get other traditional functions which are not POSIX standard.
 * Give access to low-level functions that are not portable, but are good implementing some sys utils.
 *
 */

#define _GNU_SOURCE
#include <pthread.h>
#include "libmemh.h"


/** Global static replacments for malloc, free, calloc, realloc. */
static void* (*org_malloc)(size_t) = NULL;
static void (*org_free)(void*) = NULL;
static void* (*org_realloc)(void *, size_t) = NULL;

/** Global static malloc stats: space, time and misc. */
static struct _malloc_stats *stats = NULL;
/** Global var to enable debugging. Set to 1 to see debug prints. */
static long unsigned dyn_mem_debug_en = 1;

/** Stats print helper arrays and data structures */
static char* S_STATS_STR[DYN_MEM_NO_SPACE_ALLOCS] = {
                                                "      0 - 4 bytes:",
                                                "      4 - 8 bytes:", 
                                                "     8 - 16 bytes:",
                                                "    16 - 32 bytes:",
                                                "    32 - 64 bytes:", 
                                                "   64 - 128 bytes:",
                                                "  128 - 256 bytes:", 
                                                "  256 - 512 bytes:",
                                                " 512 - 1024 bytes:",
                                                "1024 - 2048 bytes:", 
                                                "2048 - 4096 bytes:", 
                                                "4096 + more bytes:" };
static char* T_STATS_STR[DYN_MEM_NO_TIME_ALLOCS] = {
                                                "< 1    sec:",
                                                "< 10   sec:",
                                                "< 100  sec:",
                                                "< 1000 sec:",
                                                "> 1000 sec :" };

/** We are using single mutex to protect "stats" global variable. */
static pthread_mutex_t mx1 = PTHREAD_MUTEX_INITIALIZER;

/**
 * Hash table API's.
 */
/** Good heuristic Hash Function from stackOvreflow. */
int hashFunc(struct hashtable *ht, uintptr_t key) {
    /** Scramble the key: */
    unsigned int k = (unsigned int) (key & DYN_MEM_HASH_TABLE_KEY_MASK);
    k = ((k >> 16) ^ k) * 0x45d9f3b;
    k = ((k >> 16) ^ k) * 0x45d9f3b;
    k = ((k >> 16) ^ k);

    return (k % ht->size);
}

struct hashtable* createTable(int size) {
    int i;

    /** Cannot use malloc() just yet */
    struct hashtable* ht = (struct hashtable *) org_malloc(sizeof(struct hashtable));
    ht->size = size;
    //ht->list = (struct node**) org_malloc(sizeof(struct node*) * size);
    for (i = 0; i < size; i++) {
        struct node* n = (struct node*)org_malloc(sizeof(struct node));
        n->next = NULL;
        n->key = 0;
        n->val_sz = 0;
        n->val_ts = 0;
        ht->list[i] = n;
    }

    return ht;
}

/** Using Chaining (Closed Addressing Hash) to handle collisions (hopefully few max 2 per bucket). */
void insertHT(struct hashtable *ht, uintptr_t key, size_t val_sz, time_t val_ts) {
    unsigned int idx = hashFunc(ht, key);
    struct node* list = ht->list[idx];
    /** Avoid calling malloc() to break the recursion cycle. */
    struct node* newNode = (struct node*)org_malloc(sizeof(struct node));
    struct node* temp = list;
    while (temp != NULL) {
        temp = temp->next;
    }
    if (newNode == NULL) {
        fprintf(stderr, "\n%s: Error: cannot access malloc()\n", __func__);
    }
    newNode->key = key;
    newNode->val_sz = val_sz;
    newNode->val_ts = val_ts;
    newNode->next = list;
    ht->list[idx] = newNode;
}

int lookupHT(struct hashtable* ht, uintptr_t key, size_t* val_sz, time_t* val_ts, int del) {
    unsigned int idx = hashFunc(ht, key);
    struct node* list = ht->list[idx];
    struct node* temp = list;

    while (temp) {
        if ((temp->key == key) && (key != 0x0)) {
            *val_ts = temp->val_ts;
            *val_sz = temp->val_sz;
            if (del == 1) { /** TODO: delete the actual node */
                temp->key = 0x0;
                temp->val_sz = 0x0;
                temp->val_ts = 0x0;
            }
            return 1;
        }
        temp = temp->next;
    }

    return -1;
}

/**
 * Returns the index of our space stats array [0:11]
 */
static int get_space_index(size_t size) {
    if (size <= 4)
        return 0;
    else if ((size > 4) && (size <= 8))
        return 1;
    else if ((size > 8) && (size <= 16))
        return 2;
    else if ((size > 16) && (size <= 32))
        return 3;
    else if ((size > 32) && (size <= 64))
        return 4;
    else if ((size > 64) && (size <= 128))
        return 5;
    else if ((size > 128) && (size <= 256))
        return 6;
    else if ((size > 256) && (size <= 512))
        return 7;
    else if ((size > 512) && (size <= 1024))
        return 8;
    else if ((size > 1024) && (size <= 2048))
        return 9;
    else if ((size > 2048) && (size <= 4096))
        return 10;

    return 11;
}

extern void print_dyn_mem_stats(void) {
    time_t now = time(NULL);
    char* t_str = ctime(&now);
    t_str[strlen(t_str) - 2] = '\0';
    if (stats == NULL) {
        fprintf(stderr, "%s: Error: NULL Dynamic Memory Stats!\n", __func__);
        return;
    }
    
    /** Get the current time in local format. */
    fprintf(stderr, "\n>>>>>>>>>>>>>>>>>>>>>>>> %s <<<<<<<<<<<<<<<<<<<<<<<<\n", t_str);
    fprintf(stderr, "Overall Allocation Stats:\n");
    fprintf(stderr, "%lu Overall Allocations since Start\n", 
            stats->t_stats[DYN_MEM_NO_TIME_ALLOCS - 1]);
    fprintf(stderr, "%f Current Total Allocated size\n\n", 
            (float) ((1.0 * stats->s_stats[DYN_MEM_NO_SPACE_ALLOCS - 1]) / (DYN_MEM_BYTES_TO_MB)));

    fprintf(stderr, "Current allocations by size: (#  = %ld current allocations)\n",
            stats->s_stats[DYN_MEM_NO_SPACE_ALLOCS - 1] / (DYN_MEM_NO_SPACE_ALLOCS));
    for (int i = 0; i < DYN_MEM_NO_SPACE_ALLOCS - 1; i++) {
        fprintf(stderr, "%s %lu\n", S_STATS_STR[i], stats->s_stats[i]);
    }

    fprintf(stderr, "\nCurrent allocations by age: (#  = %ld current allocations)\n",
            stats->t_stats[DYN_MEM_NO_TIME_ALLOCS - 1] / (DYN_MEM_NO_TIME_ALLOCS));
    for (int i = 0; i < DYN_MEM_NO_TIME_ALLOCS - 1; i++) {
        fprintf(stderr, "%s %lu\n", T_STATS_STR[i], stats->t_stats[i]);
    }

    fprintf(stderr, "\n>>>>>>>>>>>>>>>>>>>>>>>> END <<<<<<<<<<<<<<<<<<<<<<<<\n");
    return;
}

void init_dyn_mem_stats(void) {
    pthread_mutex_lock(&mx1);
    if (dyn_mem_debug_en)
        fprintf(stderr, "\n Calling %s ... stats = %p\n", __func__, stats);
    /* Only if stats is not NULL: update it. */
    if (stats) {
        /** Create, init, and attach the hash table: */
        struct hashtable* ht = createTable(DYN_MEM_NO_HASH_TABLE_SIZE);
        stats->ht = ht;
        //stats->ht->size = DYN_MEM_NO_HASH_TABLE_SIZE;
        stats->zero_ts = stats->last_ts = time(NULL);
        stats->one_ts = stats->zero_ts + 1;
        stats->ten_ts = stats->zero_ts + 10;
        stats->hundred_ts = stats->zero_ts + 100;
        stats->thousand_ts = stats->zero_ts + 1000;
        /** Update the space stats. */
        for (int i = 0; i < DYN_MEM_NO_SPACE_ALLOCS; i++) {
            stats->s_stats[i] = 0;
        }
        /** Update the time stats. */
        for (int i = 0; i < DYN_MEM_NO_TIME_ALLOCS; i++) {
            stats->t_stats[i] = 0;
        }
    }
    pthread_mutex_unlock(&mx1);
    return;
}

/**
 * Function to update the time-based dynamic memory stats.
 * No need to lock it as it is called from one location only: dyn_mem_stats_update()
 */
void dyn_mem_time_stats_update(time_t cur_ts, time_t val_ts, size_t val_sz, int add) {
    /** Delete the allocation first. */
    if (add != 1) {
        if (val_ts <= stats->one_ts) {
            stats->t_stats[0] -= val_sz;
        }
        if (val_ts <= stats->ten_ts) {
            stats->t_stats[1] -= val_sz;
        }
        if (val_ts <= stats->hundred_ts) {
            stats->t_stats[2] -= val_sz;
        }
        if (val_ts <= stats->thousand_ts) {
            stats->t_stats[3] -= val_sz;
        } else { // if (val_ts > stats->thousand_ts)
            stats->t_stats[4] -= val_sz;
        }
        /** Update the total time stats: remove from the total */
        stats->t_stats[5] -= val_sz;
    } else {
        if (cur_ts <= stats->one_ts) {
            stats->t_stats[0] += val_sz;
        }
        if (cur_ts <= stats->ten_ts) {
            stats->t_stats[1] += val_sz;
        }
        if (cur_ts <= stats->hundred_ts) {
            stats->t_stats[2] += val_sz;
        }
        if (cur_ts <= stats->thousand_ts) {
            stats->t_stats[3] += val_sz;
        } else { // if (cur_ts > stats->thousand_ts)
            stats->t_stats[4] += val_sz;
        }
        /** Update the total time stats: add to the total */
        stats->t_stats[5] += val_sz;
    }
    
    /** Update the last timestamp. */
    stats->last_ts = cur_ts;
    /** Next, update the timestamps of the different buckets: */
    if (cur_ts >= stats->one_ts + 1) {
        stats->one_ts = cur_ts;
        stats->t_stats[0] = 0x0;    /** Reset the counter: */
    } else { /** Skip the reset ...*/
        goto end;
    }
    if (cur_ts >= stats->ten_ts + 10) {
        stats->ten_ts = cur_ts + 10;
        stats->t_stats[1] = 0x0;    /** Reset the counter: */
    } else { /** Skip the reset ...*/
        goto end;
    }
    if (cur_ts >= stats->hundred_ts + 100) {
        stats->hundred_ts = cur_ts + 100;
        stats->t_stats[2] = 0x0;    /** Reset the counter: */
    } else { /** Skip the reset ...*/
        goto end;
    }
    if (cur_ts >= stats->thousand_ts + 1000) {
        stats->thousand_ts = cur_ts + 1000;
        /** Since this means 1000 seconds had passed, move 
         *  this bucket to the last bucket. */
        stats->t_stats[4] += stats->t_stats[3];
        stats->t_stats[3] = 0x0;    /** Reset the counter: */
    }
    /** We do not reset the last time stats (> 1000) */
end:
    return;
}

/**
 * Update the stats according to the add flag: 1 add, -1 remove
 */
void dyn_mem_stats_update(uintptr_t key, size_t val_sz, time_t cur_ts, int add) {
    pthread_mutex_lock(&mx1);
    int idx = get_space_index(val_sz);
    time_t val_ts = 0x0;
    if (add == 1) {
        /** Insert a new node into the hash table: */
        insertHT(stats->ht, key, val_sz, cur_ts);
        /** Update the space summary stats */
        stats->s_stats[idx] += (long long int) val_sz;
        stats->s_stats[DYN_MEM_NO_SPACE_ALLOCS - 1] += (long long int) val_sz;
    } else { 
        /** Get the node using the key: */
        val_sz = 0x0;        
        /** Find the entry first. */
        if (lookupHT(stats->ht, key, &val_sz, &val_ts, 1) != 1) {
            if (dyn_mem_debug_en)
                fprintf(stderr, "%s: Cannot find entry: %lu (%p)\n", 
                        __func__, key, (void *) key);
            return;
        }
        /** Update the space summary stats */
        stats->s_stats[idx] -= (long long int) val_sz;
        stats->s_stats[DYN_MEM_NO_SPACE_ALLOCS - 1] -= (long long int) val_sz;
    }
    /** Update the time stats: */
    dyn_mem_time_stats_update(cur_ts, val_ts, val_sz, add);
    if (dyn_mem_debug_en && (val_sz >= DYN_MEM_BYTES_TO_MB))
        print_dyn_mem_stats();
    
    pthread_mutex_unlock(&mx1);
    return;
}

/** malloc() wrapper with stats. */
extern void* malloc(size_t size) {
    time_t now;
    if (org_malloc == NULL) {
        fprintf(stderr, "%s: Cannot find orginal malloc()\n", __func__);
        exit(1);
    }
    now = time(NULL);
    void* ptr = org_malloc(size);
    /* Invoke stats update */
    dyn_mem_stats_update((uintptr_t) ptr, size, now, 1);

    if (dyn_mem_debug_en)
        fprintf(stderr, "Calling original %s(%lu) = %p\n", __func__, size, ptr);

    return ptr;
}

/** free() wrapper with stats. */
extern void free(void* ptr) {
    time_t now;

    if (org_free == NULL) {
        fprintf(stderr, "%s: Cannot find orginal free()\n", __func__);
        exit(1);
    }
    now = time(NULL);
    if (dyn_mem_debug_en)
        fprintf(stderr, "Calling original %s(%p)\n", __func__, ptr);
    org_free(ptr);

    /* Invoke stats update */
    if (ptr != NULL)
        dyn_mem_stats_update((uintptr_t)ptr, 0, now, -1);

    return;
}

/** calloc() wrapper with stats (calling malloc()). */
extern void* calloc(size_t num, size_t size) {
    void* ptr = NULL;
    size *= num;
    if (!size)
        return NULL;
    /** calloc is using malloc. */
    if (org_malloc == NULL) {
        fprintf(stderr, "%s: Cannot find orginal calloc()/malloc()\n", __func__);
        exit(1);
    }
    /** Calling our malloc(), which in turn will call our stats update API. */
    ptr = malloc(size);
    /** Memic orginal calloc(): */
    memset(ptr, 0, size);

    return ptr;
}

/** realloc() wrapper with stats (calling malloc()/free()). */
extern void* realloc(void* ptr, size_t size) {
    time_t now;
    void* nptr = NULL;

    /** If the pointer gor freed before, then re-malloc it.*/
    if (ptr == NULL) { 
        return malloc(size);
    }

    /** Find the old allocated buffer and free it. */
    if (stats != NULL) {
        uintptr_t old_ptr = (uintptr_t) ptr;
        size_t old_sz = 0;
        time_t old_ts = 0;

        if (lookupHT(stats->ht, old_ptr, &old_sz, &old_ts, 0) != 1) {
            fprintf(stderr, "\n%s: Error could not find %p allocation in HashTable, aborting\n",
                    __func__, ptr);
            exit(1);
        }
        /** if new size and old size are the same, we are done! */
        if (old_sz == size) {
            return ptr;
        }
        /** 
         * Here we got the old timestamp, and size of the 
         * pointer. Next, we update the stats.
         */
        now = time(NULL);
        /** If the requested new size is zero, then we are done! */
        if (size == 0) {
            free(ptr);
            return NULL;
        }
        if (org_realloc == NULL) {
            fprintf(stderr, "%s: Cannot find orginal realloc()\n", __func__);
            exit(1);
        }
        nptr = org_realloc(ptr, size);
        /* Invoke stats update */
        dyn_mem_stats_update((uintptr_t)nptr, size, now, 1);
    }

    return nptr;
}

/**
 * Using GCC, executes this function (constructor) before main() function.
 */
static __attribute__((constructor)) void dyn_mem_constructor(void) {
    char* err = NULL;
    if (dyn_mem_debug_en)
        fprintf(stderr, "%s: Calling the constructor \n", __func__);
    /** Clear the errors from before (making sure no previuos error occured). */
    dlerror();
    /** Set the original malloc(): */
    org_malloc = dlsym(RTLD_NEXT, "malloc");
    if ((err = dlerror()) != NULL) {
        fprintf(stderr, "%s: malloc() Error: %s\n", __func__, err);
        exit(1);
    }
    /** Set the original free(): */
    org_free = dlsym(RTLD_NEXT, "free");
    if ((err = dlerror()) != NULL) {
        fprintf(stderr, "%s: realloc() Error: %s\n", __func__, err);
        exit(1);
    }

    org_realloc = dlsym(RTLD_NEXT, "realloc");
    if ((err = dlerror()) != NULL) {
        fprintf(stderr, "%s: realloc() Error: %s\n", __func__, err);
        exit(1);
    }

    /** Initialize dynamic memory stats.. */
    stats = (struct _malloc_stats*) org_malloc(sizeof(stats));
    if (stats == NULL) {
        fprintf(stderr, "%s: initializing Dynamic Memory Stats Error: NULL pointer\n",
                __func__);
        exit(1);
    }
    init_dyn_mem_stats();

    return;
}

static __attribute__((destructor)) void dyn_mem_destructor(void) {
    fprintf(stderr, "%s: Exiting Stats:\n", __func__);
    print_dyn_mem_stats();
    return;
}