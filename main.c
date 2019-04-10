#include "config.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutexLi = PTHREAD_MUTEX_INITIALIZER;

int hi = 3;
int Li = 20;
int rank, size;
int waiting = 0;
int Lclock = 0;
int *queue;
int my_request;

void *watek(void *data) {
    MPI_Status status;
    int msg[MSG_SIZE];
    int counter = 0;

    while (1) {
        MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        Lclock = max(Lclock, msg[2]) + 1;

        if (msg[0] == REQUEST) {
            printf("[PR] %d dostałem od %d\n", rank, status.MPI_SOURCE);
            queue[status.MPI_SOURCE] = 0;
            if (waiting == 0) {
                pthread_mutex_lock(&mutexLi);
                Li -= msg[1];
                queue[status.MPI_SOURCE] = 1;

                pthread_mutex_unlock(&mutexLi);

            }
            if (waiting > 0 && is_before_me(my_request, msg[2], rank, status.MPI_SOURCE) > 0) {
                pthread_mutex_lock(&mutexLi);
                Li -= msg[1];
                queue[status.MPI_SOURCE] = 1;
                pthread_mutex_unlock(&mutexLi);
            }
            msg[0] = ACCEPT;
            msg[2] = Lclock;
            MPI_Send(msg, MSG_SIZE, MPI_INT, status.MPI_SOURCE, MSG_HELLO, MPI_COMM_WORLD);
        } else if (msg[0] == ACCEPT) {
            printf("%d dostałem zgode od  %d moje Li=%d\n", rank, status.MPI_SOURCE, Li);
            counter += 1;
            if (counter == size - 1) {
                if (Li >= 0) {
                    counter = 0;
                    pthread_mutex_lock(&mutex);
                    pthread_cond_signal(&cond);
                    pthread_mutex_unlock(&mutex);
                } else {
                    waiting = 2;
                }
            }
        } else if (msg[0] == RETURN && queue[status.MPI_SOURCE] > 0) {
            pthread_mutex_lock(&mutexLi);
            Li += msg[1];
            queue[status.MPI_SOURCE] = 1;
            pthread_mutex_unlock(&mutexLi);
            printf("%d dostałem zwrot od  %d moje Li=%d\n", rank, status.MPI_SOURCE, Li);
            if (waiting == 2 && Li >= 0) {
                counter = 0;
                pthread_mutex_lock(&mutex);
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);
            }
        }
    }
}

void sendRequests() {
    int msg[MSG_SIZE];
    msg[0] = REQUEST;
    msg[1] = hi;
    msg[2] = my_request;
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            printf("%d wysyłam prosbe do %d\n", rank, i);
            MPI_Send(msg, MSG_SIZE, MPI_INT, i, MSG_HELLO, MPI_COMM_WORLD);
        }
    }
}

void sendResponse() {
    int msg[MSG_SIZE];
    msg[0] = RETURN;
    msg[1] = hi;
    msg[2] = Lclock;
    for (int i = 0; i < size; i++) {
        if (i != rank) {
            printf("%d zwalniam holowniki do %d\n", rank, i);
            MPI_Send(msg, MSG_SIZE, MPI_INT, i, MSG_HELLO, MPI_COMM_WORLD);
        }
    }
}

int main(int argc, char **argv) {
    pthread_t id;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    queue = malloc(sizeof(int) * size);
    reset_queue(queue, size);

    errno = pthread_create(&id, NULL, watek, (void *) (1));
    if (errno) {
        perror("pthread_create");
        return -1;
    }

    while (1) {
        // przetwarzanie lokalne
        sleep(1);

        // sekcja krytyczna
        printf("%d czekam na sekcje krytyczna\n", rank);
        // Zmniejszenie licznika o hi
        pthread_mutex_lock(&mutexLi);
        Li -= hi;
        my_request = Lclock;
        waiting = 1;
        pthread_mutex_unlock(&mutexLi);
        sendRequests(rank, hi);
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond,&mutex); // czeka aż wątek pozwoli mu na wejscie po otrzymaniu odpowiedniej liczby odpowiedzi i dobrym Li
        pthread_mutex_unlock(&mutex);
        waiting = 0;

        // Sekcja
        printf("%d IN w sekcje krytyczna\n", rank);
        sleep(1);
        // Sekcja
        printf("%d OUT w sekcje krytyczna\n", rank);

        pthread_mutex_lock(&mutexLi);
        Li += hi;
        pthread_mutex_unlock(&mutexLi);
        sendResponse();
    }
    MPI_Finalize();
}