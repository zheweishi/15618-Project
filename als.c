#include "als.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "mpi.h"

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
int* movieRating;

int* movieStartIdx;
int* userId; 
int* userRating;

#define MAX_NAME_LEN 10

struct movie* movie_hashtable; // to record the number of rating for each movie 
struct user* user_hashtable; // to record the number of rating for each user

// helper function for hashtable 
void add_movie(int id, int rating) {
    struct movie* res;
    HASH_FIND_INT(movie_hashtable, id, res);
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
    HASH_FIND_INT(movie_hashtable, id, res);
    res -> rating_num --;
}

int find_movie(int id) {
    struct movie *result;
    HASH_FIND_INT(movie_hashtable, id, result);
    if (result == NULL) {
        return 0;
    } else {
        return result -> rating_num;
    }
}

double get_movie_average(int id) {
    struct movie *result;
    HASH_FIND_INT(movie_hashtable, id, result);
    if (result == NULL) {
        return 0;
    } else {
        return result.total_rating / double(result.num);
    }
}

void add_user(int id) {
    struct user* res;
    HASH_FIND_INT(user_hashtable, id, res);
    if (res == NULL) {
        struct user* new_user = (struct user*)malloc(sizeof(struct user));
        new_user -> id = id;
        new_user -> rating_num = 1;
        HASH_ADD_INT(user_hashtable, id, new_user);
    }
}

void delete_user(int id) {
    struct user* res;
    HASH_FIND_INT(user_hashtable, id, res);
    res -> rating_num --;
}

int find_user(int id) {
    struct user *result;
    HASH_FIND_INT(user_hashtable, id, result);
    if (result == NULL) {
        return 0;
    } else {
        return result -> rating_num;
    }
}


void getInputStat(char* inputFileName) {
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
    int userId = 0;
    int movieId = 0;
    int rating = 0;
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
                char tmp[MAX_NAME_LEN] = "";
                strncpy(tmp, last_pos, word_len);
                if (comma_cnt == 0) {
                    userId = atoi(tmp);
                } else if (comma_cnt == 1) {
                    movieId = atoi(tmp);
                } else {
                    rating = atoi(tmp);
                }
                add_movie(movieId, rating);  // keep record of rating number for each movie
                add_user(movieId);  // keep record of rating number of each user
                comma_cnt++;
                last_pos = tmp + 1;
            }
            tmp++;
        }

        userNum = max(userNum, userId);
        movieNum = max(movieNum, movieId);
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

void initRatingMatrix(int userId, int movieId, int rating) {
    int movieIdx = movieStartIdx[movieId] - find_movie(movieId);
    int userIdx = userStartIdx[userId] - find_user(userId);
    userId[movieIdx] = userId;
    userRating[movieIdx] = rating;
    movieId[userIdx] = movieId;
    movieRating[userIdx] = rating;
    delete_movie(movieId);
    delete_user(userId);
}

void initMatrix() {
    movieMatrix = (feature_t *)malloc(sizeof(feature_t) * (info.numFeature * movieNum));
    userMatrix = (feature_t *)malloc(sizeof(feature_t) * (info.numFeature * userNum));
    int i = 0;
    for (i = 0; i < movieNum; ++i) {
        movieMatrix[i * info.numFeature] = get_movie_average(i + 1);
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
    movieRating = (int*)malloc(sizeof(int) * (ratingNum + 1));

    movieStartIdx = (int*)malloc(sizeof(int) * (movieNum + 1));
    userId = (int*)malloc(sizeof(int) * (ratingNum + 1));
    userRating = (int*)malloc(sizeof(int) * (ratingNum + 1));

    // init start index 
    initUserStartIndex();
    initMovieStartIndex();

    ssize_t read;
    size_t len = 0;
    char* line = NULL;
    int userId = 0;
    int movieId = 0;
    int rating = 0;
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
                char tmp[MAX_NAME_LEN] = "";
                strncpy(tmp, last_pos, word_len);
                if (comma_cnt == 0) {
                    userId = atoi(tmp);
                } else if (comma_cnt == 1) {
                    movieId = atoi(tmp);
                } else {
                    rating = atoi(tmp);
                }
                comma_cnt++;
                last_pos = tmp + 1;
            }
            tmp++;
        }

        // initialize the rating matrix (use compression)
        initRatingMatrix(userId, movieId, rating);
    }

    // initialize movie/ user matrix
    initMatrix();
}

void init(int numFeatures, int numIter, double lambda) {
    info.numIter = numIter;
    info.numFeature = numFeatures;
    info.lambda = lambda;
}

void compute(int procID, int nproc, char* inputFilename, 
             int numFeatures, int numIterations, double lambda) 
{
    const int root = 0; // set the rank 0 processor as the root 
    int tag = 0;
    int source = 0;
    MPI_State status;

    /* Read the input file and initialization */
    readInput(inputFilename);

    /* Start */

    // initialize
    init(numFeatures, numIterations, lambda);
    
    // each processor load the corresponding rating matrix of users/ movies

    // start iteration 
    int iter = 0;
    for (iter = 0; iter < numIterations; ++iter) {
        // Each processor solve user feature


        // allgather (everyone gets a local copy of U)


        // solver movie feature matrix 

        // allgather (everyone gets a local copy of M)

    }

    // compute prediction and rmse 


}