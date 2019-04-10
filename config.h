#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "mpi.h"

#define REQUEST 0
#define ACCEPT 1
#define RETURN 2
#define MSG_HELLO 100
#define MSG_SIZE 3

#define max(X, Y) (((X) > (Y)) ? (X) : (Y))
#define min(X, Y) (((X) < (Y)) ? (X) : (Y))

int is_before_me(int my_request, int his_request, int my_rank, int his_rank) {
    if (my_request == his_request) {
        return his_rank - my_rank;
    }
    return my_request - his_request;
}

void reset_queue(int *queue, int size) {
    for (int i = 0; i < size; i++) {
        queue[i] = 0;
    }
}