#include "als.h"
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>

#define MAX_NAME_LEN 10

/* Define global variables */
global_info info;

feature_t *movieMatrix; // feature matrix of movies
feature_t *userMatrix;  // feature matrix of users 

// store rating matrix in a compressed way
int userNum;
int movieNum;
int ratingNum;

int* userStartIdx;
int* movieId;
double* movieRating;

int* movieStartIdx;
int* userId; 
double* userRating;

struct movie* movie_hashtable; // to record the number of rating for each movie 
struct user* user_hashtable;   // to record the number of rating for each user

void computePredictionRMSE(gsl_matrix * M, gsl_matrix * U, gsl_matrix * R);

//---------------------------------------------
//------- helper function for hashtable--------
//--------------------------------------------- 
void add_movie(int id, double rating) {
    struct movie* res;
    HASH_FIND_INT(movie_hashtable, &id, res);
    if (res == NULL) {
        struct movie* new_movie = (struct movie*)malloc(sizeof(struct movie));
        new_movie -> id = id;
        new_movie -> rating_num = 1;
        new_movie -> num = 1;
        new_movie -> total_rating = rating;
        HASH_ADD_INT(movie_hashtable, id, new_movie);
    } else {
        res -> rating_num ++;
        res -> num ++;
        res -> total_rating += rating;
    }
}

void delete_movie(int id) {
    struct movie* res;
    HASH_FIND_INT(movie_hashtable, &id, res);
    res -> rating_num --;
}

int find_movie(int id) {
    struct movie *result;
    HASH_FIND_INT(movie_hashtable, &id, result);
    if (result == NULL) {
        return 0;
    } else {
        return result -> rating_num;
    }
}

double get_movie_average(int id) {
    struct movie *result;
    HASH_FIND_INT(movie_hashtable, &id, result);
    if (result == NULL) {
        return 0;
    } else {
        return result -> total_rating / ((double)result -> num);
    }
}

void add_user(int id) {
    struct user* res;
    HASH_FIND_INT(user_hashtable, &id, res);
    if (res == NULL) {
        struct user* new_user = (struct user*)malloc(sizeof(struct user));
        new_user -> id = id;
        new_user -> rating_num = 1;
        HASH_ADD_INT(user_hashtable, id, new_user);
    } else {
        res -> rating_num ++;
    }
}

void delete_user(int id) {
    struct user* res;
    HASH_FIND_INT(user_hashtable, &id, res);
    res -> rating_num --;
}

int find_user(int id) {
    struct user *result;
    HASH_FIND_INT(user_hashtable, &id, result);
    if (result == NULL) {
        return 0;
    } else {
        return result -> rating_num;
    }
}

//------------------------------------------------
//-----------process input data-------------------
//------------------------------------------------

/*
 * get the input relevant stat
 */
void getInputStat(char* inputFilename) {
    userNum = 0;
    movieNum = 0;
    ratingNum = 0;

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
        ratingNum++;

        // split the data line with comma
        char* tmp = line;
        char* last_pos = line;
        int word_len = 0;
        int comma_cnt = 0;

        while (comma_cnt < 3) {
            if (*tmp == ',') {
                word_len = tmp - last_pos;
                char tmpbuf[MAX_NAME_LEN] = "";
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
        add_movie(mid, rating);  // keep record of rating number for each movie
        add_user(uid);           // keep record of rating number of each user
        userNum = max(userNum, uid);
        movieNum = max(movieNum, mid);
    }
    printf("There are %d ratings, %d users, %d movies\n", ratingNum, userNum, movieNum);
}

void initUserStartIndex() {
    int i;
    int sum = 0;
    for (i = 1; i <= userNum; ++i) {
        int user_rating = find_user(i);
        userStartIdx[i - 1] = sum;
        sum += user_rating;
    }
    userStartIdx[userNum] = ratingNum;
}

void initMovieStartIndex() {
    int i;
    int sum = 0;
    for (i = 1; i <= movieNum; ++i) {
        int movie_rating = find_movie(i);
        movieStartIdx[i - 1] = sum;
        sum += movie_rating;
    }
    movieStartIdx[movieNum] = ratingNum;
}

void initRatingMatrix(int uid, int mid, double rating) {
    int movieIdx = movieStartIdx[mid] - find_movie(mid);
    int userIdx = userStartIdx[uid] - find_user(uid);
    userId[movieIdx] = uid;
    userRating[movieIdx] = rating;
    movieId[userIdx] = mid;
    movieRating[userIdx] = rating;
    delete_movie(mid);
    delete_user(uid);
}

void initMatrix() {
    printf("Init movie matrix\n");
    int i, j;
    for (i = 0; i < movieNum; ++i) {
        int start_idx = i * info.numFeatures;
        movieMatrix[start_idx] = get_movie_average(i + 1);
        // initialize the random small value
        for (j = 1; j < info.numFeatures; ++j) {
            movieMatrix[start_idx + j] = (rand() % 1000) / 1000.0;
        }
    }
}

// read input file 
// initializa the rating matrix & movie matrix 
void readInput(char* inputFilename) {
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
        printf("Start reading file, get rating details\n");
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
                char tmpbuf[MAX_NAME_LEN] = "";
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
        // initialize the rating matrix (use compression)
        initRatingMatrix(uid, mid, rating);
    }
}

// init basic arg
void init(int numFeatures, int numIter, double lambda) {
    info.numIter = numIter;
    info.numFeatures = numFeatures;
    info.lambda = lambda;
}

/*
 * allocate data structure memory 
 */
void init_data(int userNum, int movieNum, int ratingNum, int nproc) {
    printf("allocating data memory\n");

    int movieSpan = (movieNum + nproc - 1) / nproc;
    int userSpan = (userNum + nproc - 1) / nproc;

    userStartIdx = (int*)calloc(userNum + 1, sizeof(int));
    movieId =      (int*)calloc(ratingNum + 1, sizeof(int));
    movieRating =  (double*)calloc(ratingNum + 1, sizeof(double));

    movieStartIdx = (int*)calloc(movieNum + 1, sizeof(int));
    userId =        (int*)calloc(ratingNum + 1, sizeof(int));
    userRating =    (double*)calloc(ratingNum + 1, sizeof(double));

    movieMatrix = (feature_t *)calloc(info.numFeatures * movieSpan * nproc, sizeof(feature_t));
    userMatrix =  (feature_t *)calloc(info.numFeatures * userSpan * nproc, sizeof(feature_t));
}

//-----------------------------------------------------------------
//---------------------real computation----------------------------
//-----------------------------------------------------------------
void compute(int procID, int nproc, char* inputFilename, 
             int numFeatures, int numIterations, double lambda) 
{
    const int root = 0; // set the rank 0 processor as the root 
    int span;

    // initialize
    init(numFeatures, numIterations, lambda);

    /* Read the input file and initialization */
    if (procID == root) {
        getInputStat(inputFilename);
    }

    /* broadcast the information of basic stat*/
    MPI_Bcast(&userNum, 1, MPI_INT, root, MPI_COMM_WORLD);
    MPI_Bcast(&movieNum, 1, MPI_INT, root, MPI_COMM_WORLD);
    MPI_Bcast(&ratingNum, 1, MPI_INT, root, MPI_COMM_WORLD);

    // initialize the data structure
    init_data(userNum, movieNum, ratingNum, nproc);
    
    if (procID == root) {
        initUserStartIndex();
        initMovieStartIndex();
    }
    
    /* broadcast the information about start index */
    MPI_Bcast(userStartIdx, userNum + 1, MPI_INT, root, MPI_COMM_WORLD);
    MPI_Bcast(movieStartIdx, movieNum + 1, MPI_INT, root, MPI_COMM_WORLD);
   
    /* Read the input file and get detailed rating */
    if (procID == root) {
        readInput(inputFilename);
    }

    MPI_Bcast(movieId, ratingNum + 1, MPI_INT, root, MPI_COMM_WORLD);
    MPI_Bcast(movieRating, ratingNum + 1, MPI_DOUBLE, root, MPI_COMM_WORLD);
    MPI_Bcast(userId, ratingNum + 1, MPI_INT, root, MPI_COMM_WORLD);
    MPI_Bcast(userRating, ratingNum + 1, MPI_DOUBLE, root, MPI_COMM_WORLD);

    // initialize movie/ user matrix
    if (procID == root) {
        initMatrix();
    }

    MPI_Bcast(movieMatrix, numFeatures * movieNum, MPI_DOUBLE, root, MPI_COMM_WORLD);

    // initialize n_user and n_movie
    int* n_user =  (int *)malloc(sizeof(int) * userNum);
    int* n_movie = (int *)malloc(sizeof(int) * movieNum);
    int i, j;
    for (i = 0; i < userNum; ++i) {
        n_user[i] = userStartIdx[i+1] - userStartIdx[i];
    }
    for (i = 0; i < movieNum; ++i) {
        n_movie[i] = movieStartIdx[i+1] - movieStartIdx[i];
    }

    // create gsl objects
    gsl_matrix * M = gsl_matrix_alloc (numFeatures, movieNum);
    gsl_matrix * U = gsl_matrix_alloc (numFeatures, userNum);
    gsl_matrix * R = gsl_matrix_alloc (userNum, movieNum);

    gsl_matrix * A = gsl_matrix_alloc (numFeatures, numFeatures);
    gsl_matrix * Ainv = gsl_matrix_alloc (numFeatures, numFeatures);

    gsl_matrix * E = gsl_matrix_alloc (numFeatures, numFeatures);
    for (i = 0; i < numFeatures; ++i)
        gsl_matrix_set(E, i, i, 1);
    gsl_matrix * F = gsl_matrix_alloc (numFeatures, numFeatures);

    gsl_vector * V = gsl_vector_alloc (numFeatures);

    gsl_vector * Rm = gsl_vector_alloc (movieNum);
    gsl_vector * Ru = gsl_vector_alloc (userNum);

    gsl_permutation *p = gsl_permutation_alloc(numFeatures);
    gsl_vector * O = gsl_vector_alloc (numFeatures);

    /* Start */
    // start iteration
    if (procID == root) {
        printf("Start iteration\n");
        computePredictionRMSE(M, U, R);
    }

    int iter = 0;
    for (iter = 0; iter < numIterations; ++iter) {

        // Each processor solve user feature
        if (procID == root) {
            printf("Updating user (iter = %d)\n", iter);
        }

        int user_idx;
        int user_num = 0;

        // check span
        // NOTE THE NUMBER SHOULD BE DIVISIBLE
        span = (userNum + nproc - 1) / nproc;
        int user_start_idx = min(procID * span, userNum);
        int user_end_idx = min(user_start_idx + span, userNum);

        for (user_idx = user_start_idx; user_idx < user_end_idx; ++user_idx) {
            user_num++;

            gsl_matrix_set_zero (M);
            for (i = userStartIdx[user_idx]; i < userStartIdx[user_idx+1]; ++i)
                for (j = 0; j < numFeatures; ++j)
                    gsl_matrix_set(M, j, i - userStartIdx[user_idx], movieMatrix[(movieId[i]-1) * numFeatures + j]);

            gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1, M, M, 0, A);
            gsl_matrix_memcpy(F, E);
            gsl_matrix_scale(F, lambda * n_user[user_idx]);
            gsl_matrix_add(A, F); // A = M M^T + lambda * n * E

            gsl_vector_set_zero (Rm);
            for (i = userStartIdx[user_idx]; i < userStartIdx[user_idx+1]; ++i)
                gsl_vector_set(Rm, i - userStartIdx[user_idx], movieRating[i]);

            gsl_blas_dgemv(CblasNoTrans, 1, M, Rm, 0, V); // V = M R^T

            int s;
            gsl_linalg_LU_decomp(A, p, &s);
            gsl_linalg_LU_invert(A, p, Ainv);

            gsl_blas_dgemv(CblasNoTrans, 1, Ainv, V, 0, O); // O = A^-1 V

            for (j = 0; j < numFeatures; ++j)
                userMatrix[user_idx * numFeatures + j] = gsl_vector_get(O, j);
        }

        // allgather (everyone gets a local copy of U)
        MPI_Allgather(&userMatrix[user_start_idx * numFeatures], 
                      span * numFeatures, MPI_DOUBLE, 
                      userMatrix, span * numFeatures, 
                      MPI_DOUBLE, MPI_COMM_WORLD);

        // solver movie feature matrix
        if (procID == root) {
            printf("Updating movie (iter = %d)\n", iter);
        }

        int movie_idx;
        int movie_num = 0;

        span = (movieNum + nproc - 1) / nproc;
        int movie_start_idx = min(procID * span, movieNum);
        int movie_end_idx = min(movie_start_idx + span, movieNum);

        for (movie_idx = movie_start_idx; movie_idx < movie_end_idx; ++movie_idx) {
            if (movieStartIdx[movie_idx] == movieStartIdx[movie_idx+1])
                continue;

            movie_num++;
            gsl_matrix_set_zero (U);
            for (i = movieStartIdx[movie_idx]; i < movieStartIdx[movie_idx+1]; ++i)
                for (j = 0; j < numFeatures; ++j)
                    gsl_matrix_set(U, j, i - movieStartIdx[movie_idx], userMatrix[(userId[i]-1) * numFeatures + j]);

            gsl_blas_dgemm(CblasNoTrans, CblasTrans, 1, U, U, 0, A);
            gsl_matrix_memcpy(F, E);
            gsl_matrix_scale(F, lambda * n_movie[movie_idx]);
            gsl_matrix_add(A, F); // A = U U^T + lambda * n * E

            gsl_vector_set_zero (Ru);
            for (i = movieStartIdx[movie_idx]; i < movieStartIdx[movie_idx+1]; ++i)
                gsl_vector_set(Ru, i - movieStartIdx[movie_idx], userRating[i]);

            gsl_blas_dgemv(CblasNoTrans, 1, U, Ru, 0, V); // V = U R^T

            int s;
            gsl_linalg_LU_decomp(A, p, &s);
            gsl_linalg_LU_invert(A, p, Ainv);

            gsl_blas_dgemv(CblasNoTrans, 1, Ainv, V, 0, O); // O = A^-1 V

            for (j = 0; j < numFeatures; ++j)
                movieMatrix[movie_idx * numFeatures + j] = gsl_vector_get(O, j);
        }

        // allgather (everyone gets a local copy of M)
        MPI_Allgather(&movieMatrix[movie_start_idx * numFeatures], 
                      span * numFeatures, MPI_DOUBLE, 
                      movieMatrix, span * numFeatures, 
                      MPI_DOUBLE, MPI_COMM_WORLD);

        // compute prediction and rmse 
        if (procID == root) {
            computePredictionRMSE(M, U, R);
        }

    }

    gsl_matrix_free (M);
    gsl_matrix_free (U);
    gsl_matrix_free (R);
    gsl_matrix_free (A);
    gsl_matrix_free (Ainv);
    gsl_matrix_free (E);
    gsl_matrix_free (F);

    gsl_vector_free (V);
    gsl_vector_free (Rm);
    gsl_vector_free (Ru);
    gsl_vector_free (O);

    gsl_permutation_free (p);

    free(n_user);
    free(n_movie);

    free(userStartIdx);
    free(movieId);
    free(movieRating);
    free(movieStartIdx);
    free(userId);
    free(userRating);
    free(movieMatrix);
    free(userMatrix);
}

void computePredictionRMSE(gsl_matrix * M, gsl_matrix * U, gsl_matrix * R) {
    int i, j, user_idx;

    for (i = 0; i < movieNum; ++i)
        for (j = 0; j < info.numFeatures; ++j)
            gsl_matrix_set(M, j, i, movieMatrix[i * info.numFeatures + j]);

    for (i = 0; i < userNum; ++i)
        for (j = 0; j < info.numFeatures; ++j)
            gsl_matrix_set(U, j, i, userMatrix[i * info.numFeatures + j]);

    gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1, U, M, 0, R);

    double rmse = 0;

    for (user_idx = 0; user_idx < userNum; ++user_idx)
        for (i = userStartIdx[user_idx]; i < userStartIdx[user_idx+1]; ++i) {
            double diff = movieRating[i] - gsl_matrix_get(R, user_idx, movieId[i] - 1);
            rmse += diff * diff;
        }

    printf("RMSE = %f\n", rmse / ratingNum);
}
