#include "als.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
//#include "mpi.h"

#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>

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

#define MAX_NAME_LEN 10

struct movie* movie_hashtable; // to record the number of rating for each movie 
struct user* user_hashtable; // to record the number of rating for each user

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
        res -> rating_num++;
        res -> num++;
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
        res -> rating_num++;
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

/**
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
        printf("Read the tag line (not useful)\n");
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
    movieMatrix = (feature_t *)malloc(sizeof(feature_t) * (info.numFeature * movieNum));
    userMatrix = (feature_t *)malloc(sizeof(feature_t) * (info.numFeature * userNum));
    int i, j;
    for (i = 0; i < movieNum; ++i) {
        int start_idx = i * info.numFeature;
        movieMatrix[start_idx] = get_movie_average(i + 1);
        // initialize the random small value
        for (j = 1; j < info.numFeature; ++j) {
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

    // first get the needed parameter
    getInputStat(inputFilename);

    // initialize the data structure
    userStartIdx = (int*)malloc(sizeof(int) * (userNum + 1));
    movieId = (int*)malloc(sizeof(int) * (ratingNum + 1));
    movieRating = (double*)malloc(sizeof(double) * (ratingNum + 1));

    movieStartIdx = (int*)malloc(sizeof(int) * (movieNum + 1));
    userId = (int*)malloc(sizeof(int) * (ratingNum + 1));
    userRating = (double*)malloc(sizeof(double) * (ratingNum + 1));

    // init start index 
    initUserStartIndex();
    initMovieStartIndex();

    ssize_t read;
    size_t len = 0;
    char* line = NULL;
    int uid = 0;
    int mid = 0;
    double rating = 0;
    if ((read = getline(&line, &len, fp)) != -1) {
        printf("Read the tag line (not useful)\n");
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

    // initialize movie/ user matrix
    initMatrix();
}

void init(int numFeatures, int numIter, double lambda) {
    info.numIter = numIter;
    info.numFeature = numFeatures;
    info.lambda = lambda;
}

//-----------------------------------------------------------------
//---------------------real computation----------------------------
//-----------------------------------------------------------------
void compute(int procID, int nproc, char* inputFilename, 
             int numFeatures, int numIterations, double lambda) 
{
    const int root = 0; // set the rank 0 processor as the root 
    int tag = 0;
    int source = 0;
    //MPI_State status;

    // initialize
    init(numFeatures, numIterations, lambda);

    /* Read the input file and initialization */
    //if (procID == root) {
    readInput(inputFilename);
    //}

    int* n_user =  (int *)malloc(sizeof(int) * userNum);
    int* n_movie = (int *)malloc(sizeof(int) * movieNum);

    int i, j;
    for (i = 0; i < userNum; ++i) {
        n_user[i] = userStartIdx[i+1] - userStartIdx[i];
    }
    for (i = 0; i < movieNum; ++i) {
        n_movie[i] = movieStartIdx[i+1] - movieStartIdx[i];
    }

    /* broadcast the movie matrix */
    
    
    /* Start */
    // each processor load the corresponding rating matrix of users/ movies

    // start iteration
    gsl_matrix * M = gsl_matrix_alloc (numFeatures, movieNum);
    gsl_matrix * U = gsl_matrix_alloc (numFeatures, userNum);

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

    printf("Start iteration\n");

    int iter = 0;
    for (iter = 0; iter < numIterations; ++iter) {

        // Each processor solve user feature
        int user_idx;
        int user_num = 0;
        printf("Updating user (iter = %d)\n", iter);
        for (user_idx = 0; user_idx < userNum; ++user_idx) {
            user_num++;

            gsl_matrix_set_zero (M);
            for (i = userStartIdx[user_idx]; i < userStartIdx[user_idx+1]; ++i)
                for (j = 0; j < numFeatures; ++j)
                    gsl_matrix_set(M, j, i - userStartIdx[user_idx], movieMatrix[movieId[i] * numFeatures + j]);

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

        printf("Updated %d users\n", user_num);

        // allgather (everyone gets a local copy of U)

        // solver movie feature matrix
        printf("Updating movie (iter = %d)\n", iter);
        int movie_idx;
        int movie_num = 0;
        for (movie_idx = 0; movie_idx < movieNum; ++movie_idx) {
            if (movieStartIdx[movie_idx] == movieStartIdx[movie_idx+1])
                continue;

            movie_num++;
            gsl_matrix_set_zero (U);
            for (i = movieStartIdx[movie_idx]; i < movieStartIdx[movie_idx+1]; ++i)
                for (j = 0; j < numFeatures; ++j)
                    gsl_matrix_set(U, j, i - movieStartIdx[movie_idx], userMatrix[userId[i] * numFeatures + j]);

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

        printf("Updated %d movies\n", movie_num);

        // allgather (everyone gets a local copy of M)

    }

    // compute prediction and rmse 

}
