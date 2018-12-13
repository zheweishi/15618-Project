/*
 * Parallel VLSI Wire Routing via OpenMP
 * Zhewei Shi (zheweis), Weijie Chen (weijiec)
 */

#ifndef __SGD_H__
#define __SGD_H__

#include <cstdio>
#include <omp.h>


struct movie {
    int id; 
    int rating_num;
    int num;
    double total_rating;
};

struct user {
    int id; 
    int rating_num;
};

typedef struct global_info {
    int numIter;
    int numFeatures;
    double lambda;
} global_info;

typedef double feature_t;

#endif
