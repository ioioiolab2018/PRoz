#include "config.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutexLi = PTHREAD_MUTEX_INITIALIZER;

int hi = 3;
int Li = 7;
int rank, size;
int waiting = 0;
int system_clock = 0;
int request_time;
int *queue;

int isBeforeMe(int my_request_time, int his_request_time, int my_rank, int his_rank);

void resetQueue(int *queue, int size);

void sleepThread(pthread_mutex_t *mutex, pthread_cond_t *cond);

void wakeThread(pthread_mutex_t *mutex, pthread_cond_t *cond);

int drawTugboatsNumber(int tugboats_quantity);

void addTugboats(pthread_mutex_t *mutex, int *queue, int *tugboats_counter, int sender, int req_tugboats);

void removeTugboats(pthread_mutex_t *mutex, int *queue, int *tugboats_counter, int sender, int req_tugboats);

void askForTugboats(int processes_quantity, int sender, int tugboats, int clock);

void sendConsentForTugboats(int receiver, int clock);

void returnTugboats(int processes_quantity, int sender, int tugboats, int clock);

void sendMessage(int receiver, int type, int tugboats, int clock);

void *monitor(void *data) {
    MPI_Status status;
    int msg[MSG_SIZE];
    int counter = 0;

    while (1) {
        MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        pthread_mutex_lock(&mutexLi);
        system_clock = max(system_clock, msg[2]) + 1;

        if (msg[0] == REQUEST) {
            queue[status.MPI_SOURCE] = 0;
            if (waiting == 0) {
                removeTugboats(&mutexLi, queue, &Li, status.MPI_SOURCE, msg[1]);
            } else if (waiting > 0 && isBeforeMe(request_time, msg[2], rank, status.MPI_SOURCE) > 0) {
                removeTugboats(&mutexLi, queue, &Li, status.MPI_SOURCE, msg[1]);
            }
//            printf("[REQ] %d dostałem prośbę od %d: { Li: %d, counter: %d }\n", rank, status.MPI_SOURCE, Li, counter);
            sendConsentForTugboats(status.MPI_SOURCE, system_clock);
        } else if (msg[0] == ACCEPT) {
            counter += 1;
//            printf("[ACC] %d dostałem zgodę od %d: { Li: %d, counter: %d }\n", rank, status.MPI_SOURCE, Li, counter);
            if (counter == size - 1) {
                if (Li >= 0) {
                    printf("[RET] %d dostałem zgody: { Li: %d, counter: %d }\n", rank, Li, counter);
                    counter = 0;
                    wakeThread(&mutex, &cond);
                } else {
                    waiting = 2;
                }
            }
        } else if (msg[0] == RETURN && queue[status.MPI_SOURCE] > 0) {
            addTugboats(&mutexLi, queue, &Li, status.MPI_SOURCE, msg[1]);
//            printf("[RET] %d dostałem zwrot od %d: { Li: %d, counter: %d }\n", rank, status.MPI_SOURCE, Li, counter);
            if (waiting == 2 && Li >= 0) {
                printf("[RET] %d dostałem zgody: { Li: %d, counter: %d }\n", rank, Li, counter);
                counter = 0;
                wakeThread(&mutex, &cond);
            }
        }
        pthread_mutex_unlock(&mutexLi);
    }
}

int main(int argc, char **argv) {
    pthread_t id;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    queue = malloc(sizeof(int) * size);
    resetQueue(queue, size);

    hi = drawTugboatsNumber(Li);
    printf("[TUG] %d potrzebuję %d holowników\n", rank, hi);

    errno = pthread_create(&id, NULL, monitor, (void *) (1));
    if (errno) {
        perror("pthread_create");
        return -1;
    }

    while (1) {
        // przetwarzanie lokalne
        sleep(1);

        pthread_mutex_lock(&mutexLi);
        Li -= hi;
        request_time = system_clock;
        waiting = 1;
        pthread_mutex_unlock(&mutexLi);

        printf("[BIO] %d wysyłam\n", rank);
        askForTugboats(size, rank, hi, request_time);

        sleepThread(&mutex, &cond);

        waiting = 0;
        printf("%d IN sekcja krytyczna\n", rank);
        sleep(1);
        printf("%d OUT sekcja krytyczna\n", rank);

        addTugboats(&mutexLi, queue, &Li, rank, hi);
        printf("[ZWR] %d zwalniam holowniki\n", rank);
        returnTugboats(size, rank, hi, system_clock);
    }
    MPI_Finalize();
    exit(0);
}

int isBeforeMe(int my_request_time, int his_request_time, int my_rank, int his_rank) {
    if (my_request_time == his_request_time) {
        return my_rank - his_rank;
    }
    return my_request_time - his_request_time;
}

void resetQueue(int *queue, int size) {
    for (int i = 0; i < size; i++) {
        queue[i] = 0;
    }
}

void sleepThread(pthread_mutex_t *mutex, pthread_cond_t *cond) {
    pthread_mutex_lock(&*mutex);
    pthread_cond_wait(&*cond, &*mutex);
    pthread_mutex_unlock(&*mutex);
}

void wakeThread(pthread_mutex_t *mutex, pthread_cond_t *cond) {
    pthread_mutex_lock(&*mutex);
    pthread_cond_signal(&*cond);
    pthread_mutex_unlock(&*mutex);
}

int drawTugboatsNumber(int tugboats_quantity) {
    srand(time(NULL) + rank);
    return rand() % (tugboats_quantity / 2) + (tugboats_quantity / 3);
}

void addTugboats(pthread_mutex_t *mutex, int *queue, int *tugboats_counter, int sender, int req_tugboats) {
//    pthread_mutex_lock(&*mutex);
    *tugboats_counter += req_tugboats;
    queue[sender] = 0;
//    pthread_mutex_unlock(&*mutex);
}

void removeTugboats(pthread_mutex_t *mutex, int *queue, int *tugboats_counter, int sender, int req_tugboats) {
//    pthread_mutex_lock(&*mutex);
    *tugboats_counter -= req_tugboats;
    queue[sender] = 1;
//    pthread_mutex_unlock(&*mutex);
}

void askForTugboats(int processes_quantity, int sender, int tugboats, int clock) {
    for (int receiver = 0; receiver < processes_quantity; receiver++) {
        if (receiver != sender) {
            sendMessage(receiver, REQUEST, tugboats, clock);
        }
    }
}

struct Message {
    int *msg;
    int receiver;
};

void *sendAsyn(void *data) {
    struct Message *mes = (struct Message *) data;
    MPI_Send(mes->msg, MSG_SIZE, MPI_INT, mes->receiver, MSG_HELLO, MPI_COMM_WORLD);
}

void sendConsentForTugboats(int receiver, int clock) {
    pthread_t id;
    struct Message *message;
    message = malloc(sizeof(struct Message));
    int *msg = malloc(3 * sizeof(int));
    msg[0] = ACCEPT;
    msg[1] = 0;
    msg[2] = clock;
    message->msg = msg;
    message->receiver = receiver;

    errno = pthread_create(&id, NULL, sendAsyn, (void *) message);
    if (errno) {
        perror("pthread_create");
        exit(0);
    }
}

void returnTugboats(int processes_quantity, int sender, int tugboats, int clock) {
    for (int receiver = 0; receiver < processes_quantity; receiver++) {
        if (receiver != sender) {
            sendMessage(receiver, RETURN, tugboats, clock);
        }
    }
}

void sendMessage(int receiver, int type, int tugboats, int clock) {
    int msg[MSG_SIZE];
    msg[0] = type;
    msg[1] = tugboats;
    msg[2] = clock;
    MPI_Send(msg, MSG_SIZE, MPI_INT, receiver, MSG_HELLO, MPI_COMM_WORLD);
}