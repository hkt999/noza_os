#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "unity.h"
#include "nozaos.h"

/* Entry function for the heavy loading thread */
void heavy_loading_entry(void *param) {
    int i, j;
    for (i = 0; i < *(int*)param; i++) {
        for (j = 0; j < 10000000; j++) {
            /* Perform heavy computation */
            double x = rand() / (double) RAND_MAX;
            double y = rand() / (double) RAND_MAX;
            double z = x * y;
        }
        printf("Heavy loading thread %d running\n", *(int*)param);
        noza_thread_sleep(500);
    }
}

int main(int argc, char *argv[]) {
    /* Check for valid command line arguments */
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [num_iterations]\n", argv[0]);
        return EXIT_FAILURE;
    }
    int num_iterations = atoi(argv[1]);

    /* Seed random number generator */
    srand(time(NULL));

    /* Create multiple heavy loading threads */
    int num_threads = 4;
    uint32_t thread_ids[num_threads];
    int thread_args[num_threads];
    int i;
    for (i = 0; i < num_threads; i++) {
        thread_args[i] = num_iterations;
        thread_ids[i] = noza_thread_create(&heavy_loading_entry, &thread_args[i], 0);
    }

    /* Join all threads */
    for (i = 0; i < num_threads; i++) {
        noza_thread_join(thread_ids[i]);
    }

    printf("Test finish\n");

    return EXIT_SUCCESS;
}

