
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "mpi.h"
#include "als.h"

int main(int argc, char* argv[]) {
    int procID;
    int nproc;
    int opt = 0;
    char* inputFilname = NULL;
    int numIterations = 15;
    double lambda = 0.065;
    int numFeatures = 10; 
    double startTime;
    double endTime;

    // Initialize MPI 
    MPI_Init(&argc, &argv);

    // Read command line arg
    do {
        opt = getopt(argc, argv, "f:n:i:l:");
        switch (opt) {
            case 'f':
                inputFilename = optarg;
                break;
            case 'n':
                numFeatures = atoi(optarg);
                break;
            case 'i':
                numIterations = atoi(optarg);
                break;
            case 'l':
                lambda = atof(optarg);
                break; 
            case -1:
                break; 
            default: 
                break; 
        }
    } while (opt != -1);

    if (inputFilename == NULL) {
        printf("Usage: %s -f <filename> [-n <numFeatures>] [-i <N_iters>] [-l <lambda>]\n", argv[0]);
        MPI_Finalize();
        return -1;
    }

    // Get process rank 
    MPI_Comm_rank(MPI_COMM_WORLD, &procID);

    // Get total number of processes
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    // Run computation 
    startTime = MPI_Wtime();
    compute(procID, nproc, inputFilename, numFeatures, numIterations, lambda);
    endTime = MPI_Wtime();

    // Cleanup
    MPI_Finalize();
    printf("elapsed time for proc %d: %f\n", procID, endTime - startTime);
    return 0;
}
