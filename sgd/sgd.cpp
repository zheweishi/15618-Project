#include "sgd.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <assert.h>
#include <string.h>
#include <cstring>
#include <tr1/unordered_map>
#include <vector>
#include <omp.h>
#include "mic.h"

using namespace std::tr1;

#define BUFSIZE 1024

static int _argc;
static const char **_argv;


/* Starter code function, don't touch */
const char *get_option_string(const char *option_name,
                              const char *default_value)
{
  for (int i = _argc - 2; i >= 0; i -= 2)
    if (strcmp(_argv[i], option_name) == 0)
      return _argv[i + 1];
  return default_value;
}

/* Starter code function, do not touch */
int get_option_int(const char *option_name, int default_value)
{
  for (int i = _argc - 2; i >= 0; i -= 2)
    if (strcmp(_argv[i], option_name) == 0)
      return atoi(_argv[i + 1]);
  return default_value;
}

/* Starter code function, do not touch */
float get_option_float(const char *option_name, float default_value)
{
  for (int i = _argc - 2; i >= 0; i -= 2)
    if (strcmp(_argv[i], option_name) == 0)
      return (float)atof(_argv[i + 1]);
  return default_value;
}


/* Starter code function, do not touch */
static void show_help(const char *program_path)
{
    printf("Usage: %s OPTIONS\n", program_path);
    printf("\n");
    printf("OPTIONS:\n");
    printf("\t-f <input_filename> (required)\n");
    printf("\t-n <num_of_threads> (required)\n");
    printf("\t-t <num_of_feature> (required)\n");
    printf("\t-l <learning_rate> (required)\n");
    printf("\t-i <iters>\n");
}

/**
 * add the rating value into the matrix
 */
void constructRatingMatrix(const char* input_filename, feature_t* rating_matrix, int movie_num, unordered_map<int, int>& mid_map) {
    FILE* fp = fopen(input_filename, "r");   
    if (fp == NULL) {
        fprintf(stderr, "construct Rating Matrix: Failed to read input file %s\n", input_filename);
        exit(-1);
    }

    ssize_t read;
    size_t len = 0;
    char* line = NULL;
    int uid = 0;
    int mid = 0;
    double rating = 0;
    if ((read = getline(&line, &len, fp)) != -1) {
        printf("constructing: Start reading file, get input stat\n");
    }

    while ((read = getline(&line, &len, fp)) != -1) {

        // split the data line with comma
        char* tmp = line;
        char* last_pos = line;
        int word_len = 0;
        int comma_cnt = 0;

        while (comma_cnt < 3) {
            if (*tmp == ',') {
                word_len = tmp - last_pos;
                char tmpbuf[BUFSIZE] = "";
                strncpy(tmpbuf, last_pos, word_len);
                if (comma_cnt == 0) {
                    uid = atoi(tmpbuf);
                } else if (comma_cnt == 1) {
                    mid = atoi(tmpbuf);
                } else {
                    rating = atof(tmpbuf);
                }
                comma_cnt++;
                last_pos = tmp + 1;
            }
            tmp++;
        }
        // add the rating into matrix
        int idx = (uid - 1) * movie_num + mid_map[mid];
        rating_matrix[idx] = rating;
    }
}

/**
 * add movie to the hashmap
 * if exist before, ignore
 */
void add_movie(int mid, int& movie_num, unordered_map<int, int>& mid_map) {
    if (mid_map.find(mid) == mid_map.end()) {
        mid_map[mid] = movie_num;
        movie_num++;
    }
}

/**
 * get user/ movie number
 * reallocate movie ID from 0 - N
 */
void getInputStat(const char* inputFilename, int& user_num, int& movie_num, unordered_map<int, int>& mid_map) {
    FILE* fp = fopen(inputFilename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to read input file %s\n", inputFilename);
        exit(-1);
    }

    ssize_t read;
    size_t len = 0;
    char* line = NULL;
    int uid = 0;
    int mid = 0;
    double rating = 0;
    if ((read = getline(&line, &len, fp)) != -1) {
        printf("Start reading file, get input stat\n");
    }

    while ((read = getline(&line, &len, fp)) != -1) {

        // split the data line with comma
        char* tmp = line;
        char* last_pos = line;
        int word_len = 0;
        int comma_cnt = 0;

        while (comma_cnt < 3) {
            if (*tmp == ',') {
                word_len = tmp - last_pos;
                char tmpbuf[BUFSIZE] = "";
                strncpy(tmpbuf, last_pos, word_len);
                if (comma_cnt == 0) {
                    uid = atoi(tmpbuf);
                } else if (comma_cnt == 1) {
                    mid = atoi(tmpbuf);
                } else {
                    rating = atof(tmpbuf);
                }
                comma_cnt++;
                last_pos = tmp + 1;
            }
            tmp++;
        }
        add_movie(mid, movie_num, mid_map);  // keep record of rating number for each movie
        user_num = std::max(user_num, uid);
    }
    printf("There are %d users, %d movies\n", user_num, movie_num);
}

/**
 * init the matrix with initialized value
 * all small random value
 */
void initMatrix(feature_t* user_matrix, feature_t* movie_matrix, int movie_num, int user_num, int feature_num) {
    int i, j;
    for (i = 0; i < movie_num; ++i) {
        int start_idx = i * feature_num;
        // initialize the random small value
        for (j = 0; j < feature_num; ++j) {
            movie_matrix[start_idx + j] = (rand() % 1000) / 1000.0;
        }
    }

    for (i = 0; i < user_num; ++i) {
        int start_idx = i * feature_num;
        // initialize the random small value
        for (j = 0; j < feature_num; ++j) {
            user_matrix[start_idx + j] = (rand() % 1000) / 1000.0;
        }
    }
}

feature_t vector_dot(feature_t* user_vec, feature_t* movie_vec, int dimension) {
    feature_t result = 0;
    for (int i = 0; i < dimension; ++i) {
        result += user_vec[i] * movie_vec[i];
    }
    return result;
}

void update(feature_t* original, feature_t* other, feature_t lambda, 
            feature_t error, feature_t learning_rate, int dimension) {
    for (int i = 0; i < dimension; ++i) {
        feature_t tmp = error * other[i] - lambda * original[i];
        original[i] += learning_rate * tmp; 
    }
}


/**
 * sample data from the range
 * update the user/ movie matrix
 * NOTE: the smaple method is not good here!
 */
void update_user_movie_matrix(int user_start_idx, int movie_start_idx, 
                              int user_end_idx, int movie_end_idx, 
                              int movie_num, feature_t* rating_matrix, feature_t* user_matrix, 
                              feature_t* movie_matrix, feature_t lr, int feature_num) {
    int i, j;

    // tunable lambda
    feature_t lambda_user = 0.001;
    feature_t lambda_movie = 0.001;

    for (i = user_start_idx; i < user_end_idx; ++i) {
        for (j = movie_start_idx; j < movie_end_idx; ++j) {
            int idx = i * movie_num + j;
            if (rating_matrix[idx] == 0) continue;

            feature_t predicted_val = vector_dot(&user_matrix[i * feature_num], &movie_matrix[j * feature_num], feature_num);
            feature_t error_u_v = rating_matrix[idx] - predicted_val;

            feature_t* tmp_user = (feature_t*)malloc(sizeof(feature_t) * feature_num);
            memcpy(tmp_user, &user_matrix[i * feature_num], sizeof(feature_t) * feature_num);

            update(&user_matrix[i * feature_num], &movie_matrix[j * feature_num], lambda_user, error_u_v, lr, feature_num);
            update(&movie_matrix[j * feature_num], tmp_user, lambda_movie, error_u_v, lr, feature_num);

            free(tmp_user);
        }
    }
}


void computePredictionRMSE(feature_t* user_matrix, feature_t* movie_matrix, feature_t* rating_matrix, 
                           int user_num, int movie_num, int feature_num) {
    int i, j;
    feature_t rmse = 0;
    int ratingNum = 0;

    for (i = 0; i < user_num; ++i) {
        for (j = 0; j < movie_num; ++j) {
            int idx = i * movie_num + j;
            if (rating_matrix[idx] == 0) continue;
            ratingNum++;
            feature_t predicted_val = vector_dot(&user_matrix[i * feature_num], &movie_matrix[j * feature_num], feature_num);
            feature_t diff = rating_matrix[idx] - predicted_val;
            rmse += diff * diff;
        }
    }
    //printf("Rating num is %d, rmse is %f\n", ratingNum, rmse);
    printf("RMSE = %f\n", rmse / ratingNum);
}



/* =================================== */
/* ========== MAIN FUNCTION ========== */
/* =================================== */

int main(int argc, const char *argv[])
{
  using namespace std::chrono;
  typedef std::chrono::high_resolution_clock Clock;
  typedef std::chrono::duration<double> dsec;

  srand (time(NULL));

  auto init_start = Clock::now();
  double init_time = 0;

  _argc = argc - 1;
  _argv = argv + 1;

  /* You'll want to use these parameters in your algorithm */
  const char *input_filename = get_option_string("-f", NULL);
  int num_of_threads = get_option_int("-n", 1);
  int iters = get_option_int("-i", 20);
  int feature_num = get_option_int("-t", 10);
  float learning_rate = get_option_float("-l", 0.0001);

  int error = 0;

  if (input_filename == NULL) {
    printf("Error: You need to specify -f.\n");
    error = 1;
  }

  if (error) {
    show_help(argv[0]);
    return 1;
  }

  printf("Number of threads: %d\n", num_of_threads);
  printf("Number of SGD iterations: %d\n", iters);
  printf("Size of feature vector: %d\n", feature_num);
  printf("Learning rate is: %f\n", learning_rate);
  printf("Input file: %s\n", input_filename);

  unordered_map<int, int> mid_map; // map from true ID to condensed ID
  int user_num = 0;
  int movie_num = 0;

  getInputStat(input_filename, user_num, movie_num, mid_map);


  feature_t *rating_matrix = (feature_t *)calloc(movie_num * user_num, sizeof(feature_t));
  feature_t *user_matrix = (feature_t *)calloc(user_num * feature_num, sizeof(feature_t));
  feature_t *movie_matrix = (feature_t *)calloc(movie_num * feature_num, sizeof(feature_t));

  /* initialize rating/ user/ movie matrix */
  constructRatingMatrix(input_filename, rating_matrix, movie_num, mid_map);
  initMatrix(user_matrix, movie_matrix, movie_num, user_num, feature_num); 

  init_time += duration_cast<dsec>(Clock::now() - init_start).count();
  printf("Initialization Time: %lf.\n", init_time);

  auto compute_start = Clock::now();
  double compute_time = 0;

#ifdef RUN_MIC /* Distinguish between the target of compilation */
  /* This pragma means we want the code in the following block be executed in
   * Xeon Phi.
   */
#pragma offload target(mic) \
  inout(rating_matrix: length(movie_num*user_num) INOUT) \
  inout(movie_matrix: length(movie_num*feature_num) INOUT) \
  inout(user_matrix: length(user_num*feature_num) INOUT)
#endif
  {
    /** 
     * implement the DSGD here
     */

    /* set number of threads */
    num_of_threads = std::min(omp_get_num_procs(), num_of_threads);
    omp_set_num_threads(num_of_threads);
    printf("Actual of number thread %d\n", num_of_threads);

    int user_span = (user_num + num_of_threads - 1) / num_of_threads;
    int movie_span = (movie_num + num_of_threads - 1) / num_of_threads;

    printf("Before optimization\n");
    computePredictionRMSE(user_matrix, movie_matrix, rating_matrix, 
                          user_num, movie_num, feature_num);

    /* start optimization */
    for (int out_iter = 0; out_iter < iters; ++out_iter) {

        /* start subroutine */
        for (int iter = 0; iter < num_of_threads; ++iter) {

            // allocate independent blocks to different threads
            std::vector<int> user_start_idx(num_of_threads, 0);
            std::vector<int> movie_start_idx(num_of_threads, 0);
            std::vector<int> user_end_idx(num_of_threads, 0);
            std::vector<int> movie_end_idx(num_of_threads, 0);
            for (int idx = 0; idx < num_of_threads; ++idx) {
                int block_idx = (iter + idx) % num_of_threads;
                user_start_idx[idx] = std::min(block_idx * user_span, user_num);
                user_end_idx[idx] = std::min((block_idx + 1) * user_span, user_num);
                movie_start_idx[idx] = std::min(block_idx * movie_span, movie_num);
                movie_end_idx[idx] = std::min((block_idx + 1) * movie_span, movie_num);
            }

            int tidx;
        
            /* ============================== */
            /* ========start parapllel======= */
            /* ============================== */
            #pragma omp parallel for default(shared) private(tidx) schedule(dynamic) 
            for (tidx = 0; tidx < num_of_threads; ++tidx) {
                update_user_movie_matrix(user_start_idx[tidx], movie_start_idx[tidx], 
                                         user_end_idx[tidx], movie_end_idx[tidx], 
                                         movie_num, rating_matrix, user_matrix, movie_matrix, 
                                         learning_rate, feature_num);
            }
            /* Implied barrier: all threads join master thread and terminate */
            /* ============================== */
            /* ========end parapllel======= */
            /* ============================== */
        }

        /* test RMSE */
        printf("After iteration %d\n", out_iter);
        computePredictionRMSE(user_matrix, movie_matrix, rating_matrix, 
                              user_num, movie_num, feature_num);
    }
  }

  compute_time += duration_cast<dsec>(Clock::now() - compute_start).count();
  printf("Computation Time: %lf.\n", compute_time);   
}