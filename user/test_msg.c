#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include "nozaos.h"

#define NUM_SENDER_THREADS 4

/* Entry function for the sender thread */
void sender_entry(void *param) {
    int i;
    int num_iterations = *(int*)param;
    for (i = 0; i < num_iterations; i++) {
        /* Generate random message */
        noza_msg_t message;
        message.pid = 0;
        char payload[16];
        sprintf(payload, "message %d from thread %d", i, (int)pthread_self());
        message.ptr = payload;
        message.size = sizeof(payload);

        /* Send message to receiver */
        noza_call(&message);

        printf("sender thread %d sent message: %s\n", (int)pthread_self(), (char*)message.ptr);

        noza_thread_sleep(500);
    }
}

/* Entry function for the receiver thread */
void receiver_entry(void *param) {
    int i;
    int num_iterations = *(int*)param;
    for (i = 0; i < num_iterations; i++) {
        /* Receive message from sender */
        noza_msg_t message;
        noza_recv(&message);

        printf("receiver thread received message: %s\n", (char*)message.ptr);

        /* Reply to sender */
        noza_reply(&message);

        noza_thread_sleep(1000);
    }
}

int main(int argc, char *argv[]) {
    /* Parse number of iterations from command line arguments */
    int num_iterations = 10;
    if (argc > 1) {
        char *endptr;
        long arg = strtol(argv[1], &endptr, 10);
        if (endptr == argv[1] || *endptr != '\0' || arg < 1) {
            fprintf(stderr, "Invalid number of iterations: %s\n", argv[1]);
            fprintf(stderr, "Usage: %s [num_iterations]\n", argv[0]);
            return EXIT_FAILURE;
        }
        num_iterations = (int)arg;
    }

    /* seed random number generator */
    srand(time(NULL));

    /* create sender and receiver threads */
    pthread_t sender_thread_ids[NUM_SENDER_THREADS];
    pthread_t receiver_thread_id;
    int sender_args[NUM_SENDER_THREADS];
    int receiver_args = num_iterations;
    int i;
    for (i = 0; i < NUM_SENDER_THREADS; i++) {
        sender_args[i] = num_iterations;
        pthread_create(&sender_thread_ids[i], NULL, sender_entry, &sender_args[i]);
    }
    pthread_create(&receiver_thread_id, NULL, receiver_entry, &receiver_args);

    /* Join all threads */
    for (i = 0; i < NUM_SENDER_THREADS; i++) {
        pthread_join(sender_thread_ids[i], NULL);
    }
    pthread_join(receiver_thread_id, NULL);

    printf("Test finish\n");

    return EXIT_SUCCESS;
}

