#include "als.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "mpi.h"

/* Define global variables */
feature_t *movie_matrix; // feature matrix of movies
feature_t *user_matrix;  // feature matrix of users 
rate_t *rate_matrix;     // rating matrix

// read input file 
// initializa the rating matrix & movie matrix 
void readInput(char* inputFilename) {

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