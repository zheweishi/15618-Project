/*
 * Parallel VLSI Wire Routing via OpenMP
 * Zhewei Shi (zheweis), Weijie Chen (weijiec)
 */

#ifndef __SGD_H__
#define __SGD_H__

#include <cstdio>
#include <omp.h>

typedef double feature_t;

const char *get_option_string(const char *option_name,
                              const char *default_value);
int get_option_int(const char *option_name, int default_value);
float get_option_float(const char *option_name, float default_value);

/* My functions */
void constructRatingMatrix(const char* input_filename, feature_t* rating_matrix, int movie_num);
void add_movie(int mid, int& movie_num, unordered_map<int, int>& mid_map);
void getInputStat(char* inputFilename, int& user_num, int& movie_num, unordered_map<int, int>& mid_map);
void initMatrix(feature_t* user_matrix, feature_t* movie_matrix, int movie_num, int user_num, int feature_num);
feature_t vector_dot(feature_t* user_vec, feature_t* movie_vec, int dimension);
void update(feature_t* original, feature_t* other, feature_t lambda, 
            feature_t error, feature_t learning_rate, int dimension);
void update_user_movie_matrix(int user_start_idx, int movie_start_idx, 
                    int user_end_idx, int movie_end_idx, 
                    int movie_num, feature_t* rating_matrix, feature_t* user_matrix, 
                    feature_t* movie_matrix, feature_t lr, int feature_num);
void computePredictionRMSE(feature_t* user_matrix, feature_t* movie_matrix, feature_t* rating_matrix, 
                           int user_num, int movie_num, int feature_num);

#endif
