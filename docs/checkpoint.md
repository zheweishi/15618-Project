# Fast Matrix Factorization for Recommendation Systems

Team Members: Zhewei Shi (zheweis), Weijie Chen (weijiec)

Project Webpage: [https://zheweishi.github.io/15618-Project/](https://zheweishi.github.io/15618-Project/)

## Completed Work
After submitting the project proposal, we **did more research** on the topic that we are going to dive into. With sufficient background study, we started to work on the implementation of alternative least square (ALS) algorithm for recommendation systems. We **finished the implementation of ALS algorithm in both single-process model and MPI model**. We also **conducted many experiments** on the MovieLens dataset. It shows that our implementation is efficient in training while the number of processes is fewer than 16 and it gains decent performance in accuracy.

## Current Results
We downloaded a sample dataset from MovieLens website and conducted many experiments based on this dataset. Currently, we choose root mean squared error (RMSE) as the criterion for error measurement. Following are some experiment results we obtain:

(1) The training error decreases with iteration number on a single core (serial version):

(2) We use the same initialization values for all experiments. The training error on multiple cores has the identical results as those from a single core. Therefore, we can ensure the correctness of the parallel MPI version of our code.

(3) The speedup results

- Experiments on Andrew Linux servers (numIterations = 30, numFeatures = 10)

- Experiments on Latedays cluster (numIterations = 5, numFeatures = 16)

We can further analyze our experiment results:

(1) The smaller numIterations is, the better speedup we can get. The start of each iteration will depend on the results from the previous iteration. Therefore, this part is inherently serial and is hard to be parallelized.

(2) The larger dataset is, the better speedup we can get. Our approach will parallelize over the user/movie matrix, and each worker will be responsible for a subset of the matrix. Therefore, our algorithm can have an obvious performance improvement when the dataset is larger.

## Adjustments on Goals and Deliverables

Currently, we have conducted experiments on a sample dataset from movieLens. We have not done extensive and comprehensive experiments and analysis of the ALS algorithm yet. In the following weeks, we can have more time devoted to this project and we believe that we can produce all planned deliverables in our proposal. The online learning part, which we set as a bonus part, would be dependent on the SGD algorithm progress.

According to our current experiment results, we make some modifications to our goals:

 **- Plan To Achieve**
 
 (1) ALS

Implementation of sequential / parallelized ALS using MPI on Xeon CPUs

Parallelized ALS achieves nearly linear speedup before reaching **x10 (changed from X20 based on our current experiment results)**

Experiments and analysis of ALS on public dataset (MovieLens + Netflix)

(2) SGD

Implementation of sequential / parallelized SGD using OpenMP on Xeon Phi

Parallelized SGD achieves nearly linear speedup before reaching x10

Experiments and analysis of SGD on the same public dataset

**- Hope To Achieve**

Further optimization of ALS / SGD and Achieve X20 speedup **(newly added)**

Explore parallelization on online learning for recommendation systems

## Plan to Show at Poster Session

During the poster session, we plan to show:

(1) speedup performance graph for both ALS and SGD algorithms

(2) train / test error graph for both algorithms

If possible, we will spend some time exploring the online learning parallelization and we may build a demo showing the online recommendation system.

[Back](https://zheweishi.github.io/15618-Project/)
