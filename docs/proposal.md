# Fast Matrix Factorization for Recommendation Systems

Team Members: Zhewei Shi (zheweis), Weijie Chen (weijiec)

Project Webpage: [https://zheweishi.github.io/15618-Project/](https://zheweishi.github.io/15618-Project/)

## Summary
We are going to implement two parallelized algorithms for matrix factorization in recommendation systems using MPI and OpenMP frameworks. The implementation and analysis will be based on Xeon CPUs and Xeon Phi platforms.

## Background
In mathematics, matrix factorization (or matrix decomposition) is to represent a matrix by a product of matrices. It is one of the most popular collaborative filtering techniques for recommendation systems. We first briefly introduce the problem that we will be working on in this project:
* Assume that we have $N_u$ users and $N_i$ items. We are provided with an user–item interaction matrix $M \in \mathbb{R}^{N_u \times N_i}$ and its entries $m_{ui}$ can be an interaction value or a missing value (invalid). To estimate some of the missing values, we construct two matrices $U \in \mathbb{R}^{N_u \times k}$ and $I \in \mathbb{R}^{N_i \times k}$ for some rank $k$ as the latent features of users and items, so the estimated interaction value becomes the inner product of two feature vectors $\hat{m}_{ui} = U_u {I_i}^T$.
* Now it can be transformed to a matrix factorization problem. Given the interaction matrix $M$, we are required to find matrices $U$ and $I$ that best represent the existing interaction values in $M$. In practice, we will try to minimize a loss function (of $U$ and $I$) to obtain the optimal results for $U$ and $I$.

**Alternating Least Squares (ALS)** and **Stochastic Gradient Descent (SGD)** are two popular approaches to compute matrix factorization (in this case, matrices $U$ and $I$). We will discuss both algorithms and how we parallelize them, respectively.

### ALS
* Step 1: Initialize matrix $I$ by assigning average values;
* Step 2: Fix $I$, solve $U$ by minimizing the loss function;
* Step 3: Fix $U$, solve $I$ by minimizing the loss function similarly;
* Step 4: Repeat steps 2 and 3 until a stopping criterion is satisfied.

If we apply mean-square loss and append a Tikhonov regularization term on the loss function, the optimal result of $U$ while fixing $I$ can be obtained through mathematical approach. Furthermore, the calculation on each row of $U$ will be independent from each other. Then, to update $U$, we can update each row of $U$ simultaneously and therefore we can parallelize ALS by parallelizing the updates of $U$ (step 2) and of $I$ (step 3; symmetric).

### SGD
Assume that we have $v$ valid interaction values in matrix $M$ and we run SGD for $T$ rounds, we can have the following algorithm:
```
Initialize matrices U and I
for t = 1 to T:
  Draw i from {1...v} uniformly at random
  Calculate the loss of i-th interaction using current U and I
  Update U and I based on the loss
```
SGD will be harder to parallelize as the process is inherently serial. A possible way for parallelization is to allow multiple threads overwriting $U$ and $I$ matrices at the same time, assuming that the interaction matrix $M$ is sufficiently sparse. Another method is to grid the interaction matrix $M$ into many independent blocks. Each thread will only pick random pairs from the assigned block and the independence between blocks helps parallize the SGD algorithm.

## Challenge
ALS and SGD have quite different workload patterns, therefore the challenges exhibited in these two algorithms are also different.

### Workload

#### ALS

- Dependency between update steps. While updating one matrix, the other one will be fixed, therefore the updating process is inherently serial. Due to the dependency between update steps, we can only try to explore parallelization in a single update step.

- High ratio of communication to computation. While updating the user/item matrix, a worker should keep a complete and updated copy of the other matrix. If work is distributed across threads, there will be massive communications between workers when an iteration step starts/ends.

- Data compression. Fortunately, locality is not a key problem for ALS because each time the model will retrieve the complete (also continuous) feature vector of an user/item. However, we should still pay attention to how to represent and store data efficiently so that we can further utilize locality.

#### SGD

- Dependency between steps. As we can notice, the updates of the user and the item feature vectors will be dependent between different steps. So it becomes very hard to parallelize on steps. How to keep all threads busy and fully parallize the algorithm is the main challenge for SGD.

- Bad locality. Because the data pair (interaction) is randomly selected from the dataset, there will be many random memory access which can lead to a high cache-miss rate.

### System Constraints

#### ALS (Xeon CPUs + MPI)

Before each computation iteration starts, each thread should get a complete and updated copy of the data, therefore the communication cost would be huge in ALS algorithm. The communication cost between different threads and nodes will increase with the scale of the data and the number of workers involved.

#### SGD (Xeon Phi + OpenMP)

Most dataset can be fit into the memory space of Xeon Phi. So how to ensure that all threads can work cooperatively is the main challenge in our work. In addition, because of the limited cache size and the random access pattern of SGD, there will be many cache misses during computation.

## Resources

We will implement this project from scratch and we use the following papers as reference:

[1] Zhou, Yunhong, et al. "Large-scale parallel collaborative filtering for the netflix prize." International Conference on Algorithmic Applications in Management. Springer, Berlin, Heidelberg, 2008.

[2] Zhuang, Yong, et al. "A fast parallel SGD for matrix factorization in shared memory systems." Proceedings of the 7th ACM conference on Recommender systems. ACM, 2013.

[3] Recht, Benjamin, et al. "Hogwild: A lock-free approach to parallelizing stochastic gradient descent." Advances in neural information processing systems. 2011.

[4] Gemulla, Rainer, et al. "Large-scale matrix factorization with distributed stochastic gradient descent." Proceedings of the 17th ACM SIGKDD international conference on Knowledge discovery and data mining. ACM, 2011.

[5] Yu, Hsiang-Fu, et al. "Parallel matrix factorization for recommender systems." Knowledge and Information Systems 41.3 (2014): 793-819.

## Goals and Deliverables

### Plan To Achieve

#### ALS
* Implementation of sequential ALS
* Implementation of parallelized ALS using MPI on Xeon CPUs
* Parallelized ALS achieves nearly linear speedup before reaching x20
* Experiments and analysis of ALS on public dataset (MovieLens + Netflix)

#### SGD
* Implementation of sequential SGD
* Implementation of parallelized SGD using OpenMP on Xeon Phi
* Parallelized SGD achieves nearly linear speedup before reaching x10
* Experiments and analysis of SGD on the same public dataset

### Hope To Achieve

* Explore parallelization on online learning for recommendation systems

### Backoff Plan

* We will start from ALS. If the progress is not ideal, we hope to at least finish the ALS part and the implementation of SGD.

## Platform Choice

We plan to develop the whole project using C++ on the latedays cluster. C++ is a language with good trade-off between productivity and performance and it has many useful libraries for efficient matrix computations. Because we will use Xeon CPUs and Xeon Phi, the latedays cluster will be a good platform to get access to these resources.

## Tentative Schedule

| Date | Goal |
|------|------|
|10/31 |Submit Project Proposal|
|11/03 |Background Study|
|11/10 |Serial + Parallelized Implementation of ALS|
|11/17 |Experiments and Analysis of ALS|
|11/19 |Project Intermediate Checkpoint|
|11/24 |Serial + Parallelized Implementation of SGD|
|12/01 |Experiments and Analysis of SGD|
|12/08 |Online Learning Exploration|
|12/15 |Wrap Up and Final Project Submission|
