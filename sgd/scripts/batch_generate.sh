#/usr/bin/env bash
# generate jobs in batch

threads=(64 128 240) # The number of threads 
inputs=(timeinput/easy_4096.txt) # The name of the input files
rm -f *.job

for f in ${inputs[@]}
do
    for t in ${threads[@]}
    do
    ../scripts/generate_jobs.sh $f $t
    done
done