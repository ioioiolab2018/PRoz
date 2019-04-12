#include <stdio.h>
#include <stdlib.h>
#include <time.h>
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