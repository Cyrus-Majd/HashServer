
/*
 * @Author: Cyrus Majd
 *
 * HashServer program -- A server-side client used to store key-value pairs in memory.
 *
 * IMPLEMENTATION AND SHORT DESCRIPTION:
 *      This is the server code. The idea is that you connect to this server from a client, and send either a GET, SET, or DEL request.
 *      The appropriate request, along with any parameters, is then processed in a separate thread, one for each client.
 *
 *      Values are stored in a mutex-locked synchronous queue data structure. The contents of the queue is an array of structs containing
 *      a key value pair.
 *
 */


// Imports
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <pthread.h>

// Define parameters
#define QUEUESIZE 1000    // size of key-value queue
#define KEYSIZE 100
#define VALUESIZE 100

// ------------------------------- QUEUE STRUCTURE -------------------------------

// Key-Value pair structure
struct queueElement {
    char key[KEYSIZE];
    char value[VALUESIZE];
};

// Queue Structure
struct queue {
    struct queueElement data[QUEUESIZE];
    unsigned head;  // index of first item in queue
    unsigned count;  // number of items in queue
    pthread_mutex_t lock;
    pthread_cond_t read_ready;  // wait for count > 0
    pthread_cond_t write_ready; // wait for count < QUEUESIZE
};

int queue_init(struct queue *Q)
{
    Q->head = 0;
    Q->count = 0;
    int i = pthread_mutex_init(&Q->lock, NULL);
    int j = pthread_cond_init(&Q->read_ready, NULL);
    int k = pthread_cond_init(&Q->write_ready, NULL);

    if (i != 0 || j != 0 || k != 0) {
        return EXIT_FAILURE;  // obtained from the init functions (code omitted)
    }

    return EXIT_SUCCESS;
}

int queue_add(struct queue *Q, char * item)
{
    pthread_mutex_lock(&Q->lock); // make sure no one else touches Q until we're done

    while (Q->count == QUEUESIZE) {
        // wait for another thread to dequeue
        pthread_cond_wait(&Q->write_ready, &Q->lock);
        // release lock & wait for a thread to signal write_ready
    }

    // at this point, we hold the lock & Q->count < QUEUESIZE

    unsigned index = Q->head + Q->count;
    if (index >= QUEUESIZE) index -= QUEUESIZE;

    strcpy(Q->data[index].key, item);
    ++Q->count;

    pthread_mutex_unlock(&Q->lock); // now we're done
    pthread_cond_signal(&Q->read_ready); // wake up a thread waiting to read (if any)

    return 0;
}

int queue_remove(struct queue *Q, char *item)
{
    pthread_mutex_lock(&Q->lock);

    while (Q->count == 0) {
        pthread_cond_wait(&Q->read_ready, &Q->lock);
    }

    // now we have exclusive access and queue is non-empty

    item = Q->data[Q->head].key;  // write value at head to pointer
    --Q->count;
    ++Q->head;
    if (Q->head == QUEUESIZE) Q->head = 0;

    pthread_mutex_unlock(&Q->lock);

    pthread_cond_signal(&Q->write_ready);

    return EXIT_SUCCESS;
}

void queuePrint(struct queue *Q) {
    int count = Q->count;
    for (int i = 0; i < count; i++) {
        printf("Value at %d is %s\n", i, Q->data[i].key);
    }
}

int alreadyExists(struct queue *Q, char * currElement) {
    int count = Q->count;
    for (int i = 0; i < count; i++) {
        if (strcmp(Q->data[i].key, currElement) == 0) {
            // already exists, return 1
            return 1;
        }
    }
    return 0;
}

// ------------------------------- END OF QUEUE STRUCTURE -------------------------------




int main() {
    printf("Hello, World!\n");

    struct queue Q;
    queue_init(&Q);

    queue_add(&Q, "hello");
    queue_add(&Q, "pee");
    queuePrint(&Q);

    //TODO: implement value in key value pair. Handle commands. Handle connection. Handle multithreading.

    return 0;
}
