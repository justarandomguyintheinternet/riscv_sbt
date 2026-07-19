#include <stdio.h>
#include <time.h>

struct timespec start_time, end_time;

void initialise_board() {}

void start_trigger() {
    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void stop_trigger() {
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    double time_taken = (end_time.tv_sec - start_time.tv_sec) + 
                        (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
                        
    printf("Total time (secs): %f\n", time_taken);
}