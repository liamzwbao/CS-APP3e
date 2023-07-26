#include "cachelab.h"
#include <getopt.h>
#include <stdbool.h>    // booleans
#include <stdint.h>     // integer types
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

typedef struct {
    bool valid;
    uint64_t tag;
    uint64_t time_stamp;    // For LRU
} Line;

typedef struct {
    Line **lines;
    int line_number;
} Set;

typedef struct {
    Set **sets;
    int set_number;
} Cache;

typedef struct {
    int hit;
    int miss;
    int eviction;
} Result;

typedef struct {
    int s;     // Number of set index bits (S = 2^s is the number of sets)
    int E;     // Associativity (number of lines per set)
    int b;     // Number of block bits (B = 2^b is the block size)
    FILE *trace_file;
    bool verbose;
} Options;

Options *initOptions();

bool getOptions(int argc, char *argv[], Options *options);

void freeOptions(Options *options);

Result *initResult();

uint64_t getSystemTimeStamp();

Cache *initCache(Options *options);

void freeCache(Cache *cache);

void updateSet(Set *set, uint64_t tag, Result *result, bool verbose);

void accessData(Cache *cache, Options *options, Result *result);

int main(int argc, char *argv[]) {
    Options *options = initOptions();
    if (options == NULL) exit(EXIT_FAILURE);
    if (getOptions(argc, argv, options) == false) {
        freeOptions(options);
        exit(EXIT_FAILURE);
    }

    Result *result = initResult();
    if (result == NULL) {
        freeOptions(options);
        exit(EXIT_FAILURE);
    }

    Cache *cache = initCache(options);
    if (cache == NULL) {
        freeOptions(options);
        free(result);
        exit(EXIT_FAILURE);
    }

    accessData(cache, options, result);
    printSummary(result->hit, result->miss, result->eviction);

    freeOptions(options);
    free(result);
    freeCache(cache);

    return 0;
}

Options *initOptions() {
    Options *options = (Options *) malloc(sizeof(Options));
    if (options == NULL) {
        printf("Memory not allocated for options.\n");
        return NULL;
    }
    options->s = 0;
    options->E = 0;
    options->b = 0;
    options->trace_file = NULL;
    options->verbose = false;
    return options;
}

bool getOptions(int argc, char *argv[], Options *options) {
    const char *help_message =
            "Usage: ./csim [-h] -s <num> -E <num> -b <num> -t <file>\n"
            "Options:\n"
            "\t-h         Print this help message.\n"
            "\t-v         Optional verbose flag.\n"
            "\t-s <num>   Number of set index bits.\n"
            "\t-E <num>   Number of lines per set.\n"
            "\t-b <num>   Number of block offset bits.\n"
            "\t-t <file>  Trace file.\n"
            "\n"
            "Examples:\n"
            "\tlinux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n";
    const char *shortopts = "hvs:E:b:t:";

    char opt;
    while ((opt = getopt(argc, argv, shortopts)) != -1) {
        switch (opt) {
            case 'h':
                printf("%s", help_message);
                return true;
            case 'v':
                options->verbose = true;
                break;
            case 's':
                int set_bits = atoi(optarg);
                if (set_bits <= 0) {
                    printf("./csim: Number of set index bits should be positive\n");
                    printf("%s", help_message);
                    return false;
                }
                options->s = set_bits;
                break;
            case 'E':
                int line_number = atoi(optarg);
                if (line_number <= 0) {
                    printf("./csim: Number of lines per set should be positive\n");
                    printf("%s", help_message);
                    return false;
                }
                options->E = line_number;
                break;
            case 'b':
                int block_bits = atoi(optarg);
                if (block_bits <= 0) {
                    printf("./csim: Number of block offset bits should be positive\n");
                    printf("%s", help_message);
                    return false;
                }
                options->b = block_bits;
                break;
            case 't':
                if ((options->trace_file = fopen(optarg, "r")) == NULL) {
                    perror("Failed to open trace file");
                    return false;
                }
                break;
            default:
                printf("%s", help_message);
                return false;
        }
    }

    if (options->s == 0 || options->E == 0 || options->b == 0 || options->trace_file == NULL) {
        printf("./csim: Missing required command line argument\n");
        return false;
    }

    return true;
}

void freeOptions(Options *options) {
    if (options == NULL) {
        return;
    }
    if (options->trace_file != NULL) {
        fclose(options->trace_file);
    }
    free(options);
}

Result *initResult() {
    Result *result = (Result *) malloc(sizeof(Result));
    if (result == NULL) {
        printf("Memory not allocated for result.\n");
        return NULL;
    }
    result->hit = 0;
    result->miss = 0;
    result->eviction = 0;
    return result;
}

uint64_t getSystemTimeStamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

Cache *initCache(Options *options) {
    const char *error_message = "Cache initialisation failed.\n";

    Cache *cache = (Cache *) malloc(sizeof(Cache));

    if (cache == NULL) {
        printf("%s", error_message);
        return NULL;
    }
    cache->set_number = 1 << options->s;
    cache->sets = (Set **) malloc(sizeof(Set *) * cache->set_number);

    if (cache->sets == NULL) {
        printf("%s", error_message);
        freeCache(cache);
        return NULL;
    }
    for (int i = 0; i < cache->set_number; i++) {
        cache->sets[i] = (Set *) malloc(sizeof(Set));
        Set *set = cache->sets[i];
        if (set == NULL) {
            printf("%s", error_message);
            freeCache(cache);
            return NULL;
        }
        set->line_number = options->E;
        set->lines = (Line **) malloc(sizeof(Line *) * set->line_number);

        if (set->lines == NULL) {
            printf("%s", error_message);
            freeCache(cache);
            return NULL;
        }
        for (int j = 0; j < set->line_number; j++) {
            set->lines[j] = (Line *) malloc(sizeof(Line));
            Line *line = set->lines[j];
            if (line == NULL) {
                printf("%s", error_message);
                freeCache(cache);
                return NULL;
            }
            line->valid = false;
            line->tag = -1;
            line->time_stamp = getSystemTimeStamp();
        }
    }

    return cache;
}

void freeCache(Cache *cache) {
    if (cache == NULL) {
        return;
    }

    if (cache->sets != NULL) {
        for (int i = 0; i < cache->set_number; i++) {
            Set *set = cache->sets[i];
            if (set == NULL) continue;
            if (set->lines != NULL) {
                for (int j = 0; j < set->line_number; j++) {
                    Line *line = set->lines[j];
                    if (line != NULL) free(set->lines[j]);
                }
                free(set->lines);
            }
            free(set);
        }
        free(cache->sets);
    }
    free(cache);
}

void updateSet(Set *set, uint64_t tag, Result *result, bool verbose) {
    // hit
    for (int i = 0; i < set->line_number; i++) {
        Line *line = set->lines[i];
        if (line->valid && line->tag == tag) {
            if (verbose) printf(" hit");
            result->hit++;
            line->time_stamp = getSystemTimeStamp();
            return;
        }
    }

    // miss
    if (verbose) printf(" miss");
    result->miss++;
    for (int i = 0; i < set->line_number; i++) {
        Line *line = set->lines[i];
        if (!line->valid) {
            line->valid = true;
            line->tag = tag;
            line->time_stamp = getSystemTimeStamp();
            return;
        }
    }

    // eviction
    if (verbose) printf(" eviction");
    result->eviction++;
    uint64_t oldestTimeStamp = UINT64_MAX;
    int oldestLine = 0;
    for (int i = 0; i < set->line_number; i++) {
        Line *line = set->lines[i];
        if (line->time_stamp < oldestTimeStamp) {
            oldestTimeStamp = line->time_stamp;
            oldestLine = i;
        }
    }
    set->lines[oldestLine]->tag = tag;
    set->lines[oldestLine]->time_stamp = getSystemTimeStamp();
}

void accessData(Cache *cache, Options *options, Result *result) {
    int set_index_bits = options->s;
    int block_bits = options->b;
    char operation;
    uint64_t address;
    int size;
    uint64_t set_index_mask = (1 << set_index_bits) - 1;

    while ((fscanf(options->trace_file, " %c %lx,%d\n", &operation, &address, &size)) == 3) {
        if (options->verbose) {
            printf("%c %lx,%d", operation, address, size);
        }
        uint64_t set_index = (address >> block_bits) & set_index_mask;
        Set *set = cache->sets[set_index];
        uint64_t tag = address >> (block_bits + set_index_bits);
        switch (operation) {
            case 'I':
                break;
            case 'L':
                updateSet(set, tag, result, options->verbose);
                break;
            case 'M':
                updateSet(set, tag, result, options->verbose);
            case 'S':
                updateSet(set, tag, result, options->verbose);
                break;
        }
        if (options->verbose) {
            printf("\n");
        }
    }
}
