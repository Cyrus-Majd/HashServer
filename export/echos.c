
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
 *      a key value pair. This is the data structure with which the client interacts. The program handles three commands: "SET", "GET", & "DEL".
 *
 *      "SET" [length] [key] [value]
 *          Sets a key-value pair in the synchronous queue structure. If a pair with the same key already exists, the old pair is deleted and the
 *          new pair takes its place.
 *      "GET" [length] [key]
 *          Returns the value at the key in the synchronous queue structure. If the key-value pair does not exist, KNF is returned.
 *      "DEL" [length] [key]
 *          Deletes the key-value pair specified. If it does not exists, KNF is returned.
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
#include <netinet/in.h>
#include <signal.h>

// Define parameters
#define QUEUESIZE 1000000    // size of key-value queue
#define KEYSIZE 100
#define VALUESIZE 100
#define DEBUG_QUEUE 0
#define SERVER_PORT 18000
#define SERVER_BACKLOG 100
#define MAXLINE 4096
#define DEBUG_SOCKETS 1
#define SA struct sockaddr

// ------------------------------- QUEUE STRUCTURE -------------------------------

// Key-Value pair structure
struct queueElement {
    char key[KEYSIZE];
    char value[VALUESIZE];
};

// Queue Structure
struct queue {
    struct queueElement* data;
    unsigned head;  // index of first item in queue
    unsigned count;  // number of items in queue
    pthread_mutex_t lock;
    pthread_cond_t read_ready;  // wait for count > 0
    pthread_cond_t write_ready; // wait for count < QUEUESIZE
};

// holds arguements for multithreaded call to connection()
struct arg_struct {
    int connfd_STRUCT;
    struct queue *Q;
    int n;
};

// Method definitions
int queue_init(struct queue *Q);
int queue_add(struct queue *Q, char * key, char * value);
int queue_remove(struct queue *Q, char *item);
int queue_remove_UNLOCKED(struct queue *Q, char *item);
void queuePrint(struct queue *Q);
int indexOfElement(struct queue *Q, char * currElement);
int alreadyExists(struct queue *Q, char * currElement);
void queueDestroy(struct queue *Q);
int commandHandler(char * command);
char* bin2hex(const unsigned char *input, size_t len);

int queue_init(struct queue *Q)
{
    Q->data = malloc(QUEUESIZE * sizeof(struct queueElement));
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

int queue_add(struct queue *Q, char * key, char * value)
{

    pthread_mutex_lock(&Q->lock); // make sure no one else touches Q until we're done

    while (Q->count == QUEUESIZE) {
        // wait for another thread to dequeue
        pthread_cond_wait(&Q->write_ready, &Q->lock);
        // release lock & wait for a thread to signal write_ready
    }

    // at this point, we hold the lock & Q->count < QUEUESIZE

    // prevent duplicate keys
    if (alreadyExists(Q, key)){
        printf("Deleting the duplicate element!\n");
        queue_remove_UNLOCKED(Q, key);
    }

    unsigned index = Q->head + Q->count;
    if (index >= QUEUESIZE) index -= QUEUESIZE;

    strcpy(Q->data[index].key, key);
    strcpy(Q->data[index].value, value);
    ++Q->count;

    pthread_mutex_unlock(&Q->lock); // now we're done
    pthread_cond_signal(&Q->read_ready); // wake up a thread waiting to read (if any)

    return 0;
}

int queue_remove_UNLOCKED(struct queue *Q, char *item)
{
//    printf("ONE\n");

//    printf("TWO\n");
    while (Q->count == 0) {
        pthread_cond_wait(&Q->read_ready, &Q->lock);
    }
//    printf("THREE\n");
    // now we have exclusive access and queue is non-empty

    // check if key exists, if it does, delete the element and shift everything else over.
    if (alreadyExists(Q, item)) {
        int index = indexOfElement(Q, item);
        --Q->count;
        for (int i = index; i < Q->count; i++) {
            Q->data[i] = Q->data[i+1];
        }
    }
        // in case the key does not exist
    else {
        perror("ERROR: key-not-found!\n");
    }
//    printf("FOUR\n");
//    --Q->head;
    if (Q->head == QUEUESIZE) Q->head = 0;

//    printf("FIVE\n");

//    printf("SIX\n");
    return EXIT_SUCCESS;
}

int queue_remove(struct queue *Q, char *item)
{
//    printf("ONE\n");
    pthread_mutex_lock(&Q->lock);

//    printf("TWO\n");
    while (Q->count == 0) {
        pthread_cond_wait(&Q->read_ready, &Q->lock);
    }
//    printf("THREE\n");
    // now we have exclusive access and queue is non-empty

    // check if key exists, if it does, delete the element and shift everything else over.
    if (alreadyExists(Q, item)) {
        int index = indexOfElement(Q, item);
        --Q->count;
        for (int i = index; i < Q->count; i++) {
            Q->data[i] = Q->data[i+1];
        }
    }
    // in case the key does not exist
    else {
        perror("ERROR: key-not-found!\n");
    }
//    printf("FOUR\n");
//    --Q->head;
    if (Q->head == QUEUESIZE) Q->head = 0;

//    printf("FIVE\n");
    pthread_mutex_unlock(&Q->lock);
    pthread_cond_signal(&Q->write_ready);

//    printf("SIX\n");
    return EXIT_SUCCESS;
}

char* queue_get(struct queue *Q, char *key) {
    int count = Q->count;
    for (int i = 0; i < count; i++) {
        if (strcmp(Q->data[i].key, key) == 0) {
            return Q->data[i].value;
        }
    }
}

void queuePrint(struct queue *Q) {
    int count = Q->count;
    for (int i = 0; i < count; i++) {
        printf("Value at %d: KEY IS \'%s\' VALUE IS \'%s\'\n", i, Q->data[i].key, Q->data[i].value);
    }
}

int indexOfElement(struct queue *Q, char * currElement) {
    int count = Q->count;
    for (int i = 0; i < count; i++) {
        if (strcmp(Q->data[i].key, currElement) == 0) {
            return i;
        }
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

void queueDestroy(struct queue *Q) {
    free(Q->data);
}

// ------------------------------- END OF QUEUE STRUCTURE -------------------------------

// ------------------------------- HANDLING COMMANDS -------------------------------

char* bin2hex(const unsigned char *input, size_t len) {
    char * result;
    char * hexits = "0123456789ABCDEF";
    if (input == NULL || len <= 0) {
        return NULL;
    }

    int resultlength = (len*3) + 1;

    result = malloc(resultlength);
    bzero(result, resultlength);

    for (int i = 0; i < len; i++) {
        result[i*3] = hexits[input[i] >> 4];
        result[(i*3) + 1] = hexits[input[i] & 0x0F];
        result[(i*3) + 2] = ' ';
    }

    return result;
}

int commandHandler(char * command) {
//    printf("read command: %s. Compared to DEL and DELnewline: %d %d\n", command, strcmp(command, "DEL"), strcmp(command, "DEL\n"));
    if (strcmp(command, "SET") == 0 || strcmp(command, "SET\n") == 0) {
        return 0;
    }
    else if (strcmp(command, "GET") == 0 || strcmp(command, "GET\n") == 0) {
        return 1;
    }
    else if (strcmp(command, "DEL") == 0 || strcmp(command, "DEL\n") == 0) {
        return 2;
    }
    else {
        return 3;
    }

}

// handles cntrl C when the server wants to end.
void sig_handler(int signum){
    //Return type of the handler function should be void
    printf("\nInside handler function\n");
}

// ------------------------------- END OF HANDLING COMMANDS -------------------------------

void * connection(void *arguements) {
//    struct arg_struct *args = malloc(sizeof (struct arg_struct));
//    args = (struct arg_struct *) arguements;
//    sleep(2);
//    printf("copied struct\n");

    int connfd = ((struct arg_struct *) arguements)->connfd_STRUCT;
//    sleep(2);
//    printf("copied FD: %d\n", connfd);

    int n = ((struct arg_struct *) arguements)->n;
//    sleep(2);
//    printf("copied n: %d\n", n);

    struct queue *Q = ((struct arg_struct *) arguements)->Q;
//    sleep(2);
//    printf("copied queue\n");

    char buff[MAXLINE + 1];
    int escape = 0;
//            signal(SIGINT,sig_handler);
    while (escape == 0) {
        // read client message
        int counter = 0;
        int limit = 0;
        int commandType;
        char paramOne[1000] = "";
        char paramTwo[1000] = "";
        int msgLength = 0;
        char recvline[MAXLINE + 1];
        while ((n = read(connfd, recvline, MAXLINE - 1)) > 0) {
            char word[1000] = "";
//            fprintf(stdout, "\nRECVLINE: %s WORD: %s\n", recvline, word);
//            strcpy(word, recvline);
            strncpy(word, recvline, strlen(recvline) - 2);
            strcpy(recvline, "");
            strcat(word, "");
            if (counter == 0) {
                char substr[7];
                strncpy(substr, word, 3);
                strcpy(word, substr);
            }
//                    fprintf(stdout, "\nRECVLINE: %s WORD: %s\n", recvline, word);
//            printf("==========%s=========", word);
            if (word[strlen(word) - 1] == '\n') {
//                printf("NEWLINE DETECTED!\n");
                word[strlen(word) - 2] = '\0';
            }

            if (word[0] == -1 && word[1] == -12 && word[2] == -1) {
//                printf("CTRL + C DETECTED!\n");
                escape = 1;
                break;
            }

            if (counter == 0) {
//                printf("TESING: %s", recvline);
                int tmp = commandHandler(word);
                if (tmp == 0) { // we have a SET, so 4 arguements (including "SET")
                    limit = 2;
                    commandType = 0;
                } else if (tmp == 1 || tmp == 2) {
                    limit = 1; // either a GET or a DEL
                    if (tmp == 1) {
                        commandType = 1;
                    } else {
                        commandType = 2;
                    }
                } else {
                    printf("%d%d%d", word[0], word[1], word[2]);
                    if (word[0] == -1 && word[1] == -12 && word[2] == -1) {
                        //ctrl + C is pressed! exit the thread and close connection.
                    }
                    perror(" INVALID COMMAND!\n");
//                    snprintf((char *) buff, sizeof(buff), "INVALID COMMAND! Ending Program.\n");
//                            queueDestroy(&Q);
                    write(connfd, "ERR\nBAD\n", strlen("ERR\nBAD\n"));
                    escape = 1;
                    break;
                }
            }
            if (counter == 1) {
                char numWord[10] = "";
                strncpy(numWord, word, strlen(word));
                msgLength = atoi(numWord);
//                printf("NUM WORD: %d\n", msgLength);
            }
            if (counter == 2) {
                strcpy(paramOne, word);
            }
            if (counter == 3) {
                strcpy(paramTwo, word);
            }
            if (counter > limit) {
                break;
            }

            memset(recvline, 0, MAXLINE);
            recvline[0] = '\0';
            strcpy(recvline, "");
//                    printf("CONTENTS OF RECV AFTER CLEAN: %s", recvline);
            counter++;
        }

        if (escape) {
            continue;
        }

        if (n < 0) {
            perror("read error!\n");
            queueDestroy(Q);
            escape = 1;
            break;
        }

//                // response
//                snprintf((char *) buff, sizeof(buff), "HTTP/1.0 200 OK\r\n\r\nHello :)");

        if (commandType == 0) {
            if (msgLength != (int)(strlen(paramOne) + strlen(paramTwo)) + 2) {
                write(connfd, "ERR\nLEN\n", strlen("ERR\nLEN\n"));
                escape = 1;
                break;
            }
            else {
                //            printf("TOTAL LENGHT OF WORDS: %d",(int)(strlen(paramOne) + strlen(paramTwo)));
                queue_add(Q, paramOne, paramTwo);
                // response
//                snprintf((char *) buff, sizeof(buff), "Added pair '%s':'%s'\n", paramOne, paramTwo);
                write(connfd, "OKS\n", strlen("OKS\n"));
            }
        } else if (commandType == 1) {
            if (msgLength != (int) strlen(paramOne) + 1) {
                write(connfd,  "ERR\nLEN\n", strlen("ERR\nLEN\n"));
                escape = 1;
                break;
            }
            else {
                // if the element exists or not. if doesnt, return key not found (KNF) error
                if (alreadyExists(Q, paramOne)) {
                    printf("KEY %s HAS VALUE %s\n", paramOne, queue_get(Q, paramOne));
                    // response
//                    snprintf((char *) buff, sizeof(buff), "Key %s has value %s\n", paramOne, queue_get(Q, paramOne));
                    char tmpGetStr[100] = "";
                    strcpy(tmpGetStr, queue_get(Q, paramOne));
                    strcat(tmpGetStr, "\n");
                    write(connfd, "OKG\n", strlen("OKG\n"));
                    char lenBuff[20] = "";
                    sprintf(lenBuff, "%d", (int) strlen(tmpGetStr));
                    strcat(lenBuff, "\n");
                    write(connfd, lenBuff, 17);
                    write(connfd, tmpGetStr, strlen(tmpGetStr));
                } else {
                    write(connfd, "KNF\n", strlen("KNF\n"));
                }
            }
        } else {
            if (msgLength != (int) strlen(paramOne) + 1) {
                write(connfd,  "ERR\nLEN\n", strlen("ERR\nLEN\n"));
                escape = 1;
                break;
            }
            else {
                // response
                if (alreadyExists(Q, paramOne)) {
                    snprintf((char *) buff, sizeof(buff), "Removed pair at key %s\n", paramOne);
                    write(connfd, "OKD\n", strlen("OKD\n"));
                    char lenBuff[20] = "";
                    char tmpGetStr[100] = "";
                    strcpy(tmpGetStr, queue_get(Q, paramOne));
                    strcat(tmpGetStr, "\n");
                    queue_remove(Q, paramOne);
                    sprintf(lenBuff, "%d", (int) strlen(tmpGetStr));
                    strcat(lenBuff, "\n");
                    write(connfd, lenBuff, 17);
                    write(connfd, tmpGetStr, strlen(tmpGetStr));
                } else {  //return KNF if key is not found.
                    write(connfd, "KNF\n", strlen("KNF\n"));
                }
            }
        }

        printf("CURRENT CONTENTS OF THE QUEUE:\n");
        queuePrint(Q);
        printf("=============================\n\n");

    }
    // closing things
    write(connfd, (char *) buff, strlen((char *) buff));
//    printf("CLOSING OLD CONNECTION.\n");
    close(connfd);
    free(arguements);
}

int main(int argc, char *argv[argc]) {
//    printf("Hello, World!\n");

    // check arguements. there must be only one, and it must be an integer.
    if (argc != 2) {
        perror("NOT ENOUGH ARGUEMENTS PROVIDED!\n");
        return EXIT_FAILURE;
    }
    int serverPort = atoi(argv[1]);

    int listenfd, connfd, n;
    struct sockaddr_in servaddr;

    // allocate a socket
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket allocation error!\n");
        return EXIT_FAILURE;
    }

    // setting up the address
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(serverPort);

    // listen and bind
    if ((bind(listenfd, (SA *) &servaddr, sizeof(servaddr)) < 0)) {
        perror("bind allocation error!\n");
        return EXIT_FAILURE;
    }

    if (listen(listenfd, SERVER_BACKLOG) < 0) {
        perror("listening error!\n");
        return EXIT_FAILURE;
    }

    struct queue Q;
    queue_init(&Q);

    if (DEBUG_SOCKETS) {
        // endless loop POG. listening for connections B)
        // macro to suppress annoying infinite loop warnings. like duh i know im in an infinite loop silly compooter it is by DESIGN
        #pragma clang diagnostic push
        #pragma ide diagnostic ignored "EndlessLoop"
        for (;;) {
            struct sockaddr_in addr;
            socklen_t addr_len;

            // accept until connection arrives, returns to connfd when connection is made
            printf("Waiting for a connection on port %d\n", serverPort);
            fflush(stdout);
            connfd = accept(listenfd, (SA *) NULL, NULL);

            // MULTITHREADING: Each new connection gets its own thread, so message overlapping does not occur. also efficient. and cool.

            pthread_t t;
            n = 0;
            //TODO: something is wrong with the following line. fix it, and we will have multithreading.
            //HYPOTHESIS: it has something to do with lines 461 thru 466, this is where args is constantly redifined. our old threads lose a pointer to it.
            struct arg_struct *args2 = malloc(sizeof (struct arg_struct));
            args2->connfd_STRUCT = connfd;
            args2->Q = &Q;
            args2->n = n;
            pthread_create(&t, NULL, connection, (void *)args2);
//            sleep(15);
//            connection((void* )&args);
        }
        #pragma clang diagnostic pop
    }

    if (DEBUG_QUEUE) {
        struct queue Q;
        queue_init(&Q);
        queue_add(&Q, "key1", "value1");
        queue_add(&Q, "key2", "value2");
        queue_add(&Q, "key3", "value3");
        queue_add(&Q, "key4", "value4");
        queue_add(&Q, "key5", "value5");
        queue_add(&Q, "key6", "value6");
        queuePrint(&Q);

        queue_remove(&Q, "key1");
        queue_add(&Q, "key1", "value1");
        queue_add(&Q, "key7", "value7");

        printf("\n");

        queuePrint(&Q);

        printf("VALUE OF KEY3: %s", queue_get(&Q, "key3"));
    }

    queueDestroy(&Q);

    //TODO: Implement dyanmically allocated data array. Implement value in key value pair. Handle commands. Handle connection. Handle multithreading.
    //                  DONE                                        DONE                        DONE                DONE
    return 0;
}
