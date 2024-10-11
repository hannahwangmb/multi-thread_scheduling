#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

// Define a condition variable to signal that all threads have been created
pthread_cond_t thread_created_cond;
pthread_mutex_t thread_count_mutex;

// Define separate mutexes for each queue
pthread_mutex_t eastbound_priority_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t eastbound_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t westbound_priority_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t westbound_mutex = PTHREAD_MUTEX_INITIALIZER;

// Define a mutex for printing
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// count number of threads created
int thread_count = 0;
// count number of lines from input.txt
int total_trains = 0;
// count number of trains in each queue
int eastbound_priority_count = 0;
int eastbound_count = 0;
int westbound_priority_count = 0;
int westbound_count = 0;
int arrival_count = 0;

// Define the queues and their respective counts
struct Train* eastbound_priority;
struct Train* eastbound;
struct Train* westbound_priority;
struct Train* westbound;
struct Train* arrival_order;

struct timespec start_time;

struct Train
{
    int train_no;
    char direction;
    int loading_time;
    int crossing_time;
};

int countLines(FILE* fp) {
    int count = 0;
    char direction;
    int loading_time, crossing_time;
    while (fscanf(fp, " %c %d %d", &direction, &loading_time, &crossing_time) == 3) {
        count++;
    }
    if (count == 0) {
        printf("error finding trains from input file\n");
        exit(1);
    }
    // Reset file pointer to the beginning of the file
    fseek(fp, 0, SEEK_SET);
    return count;
}

void setStartTime() {
    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void printTimeDifference() {
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);

    long seconds = current_time.tv_sec - start_time.tv_sec;
    long nanoseconds = current_time.tv_nsec - start_time.tv_nsec;

    if (nanoseconds < 0) {
        seconds--;
        nanoseconds += 1000000000;
    }
    int milliseconds = (int)(nanoseconds / 1000000);
    int rounded_milliseconds = (milliseconds + 5) / 100;

    printf("%02ld:%02ld:%02ld.%01d ", seconds / 3600, (seconds % 3600) / 60, seconds % 60, rounded_milliseconds);

}

void initializeQueues(int total_trains) {
    eastbound_priority = malloc(total_trains * sizeof(struct Train));
    eastbound = malloc(total_trains * sizeof(struct Train));
    westbound_priority = malloc(total_trains * sizeof(struct Train));
    westbound = malloc(total_trains * sizeof(struct Train));
    arrival_order = malloc(total_trains * sizeof(struct Train));
}

void addToArrivalQueue(struct Train train) {
        arrival_order[arrival_count] = train;
        arrival_count++;
}

void printEastReady(struct Train train){
    pthread_mutex_lock(&print_mutex);
    printTimeDifference();
    printf("Train %2d is ready to go East\n", train.train_no); 
    pthread_mutex_unlock(&print_mutex);
}

void printWestReady(struct Train train){
    pthread_mutex_lock(&print_mutex);
    printTimeDifference();
    printf("Train %2d is ready to go West\n", train.train_no); 
    pthread_mutex_unlock(&print_mutex);
}

// Function to enqueue a loaded train into the appropriate queue
void enqueue(struct Train train) {
    struct Train* queue;
    int* queue_count;
    pthread_mutex_t* queue_mutex;
    
    if (train.direction == 'E') {
        queue = eastbound_priority;
        queue_count = &eastbound_priority_count;
        queue_mutex = &eastbound_priority_mutex;
    } else if (train.direction == 'e') {
        queue = eastbound;
        queue_count = &eastbound_count;
        queue_mutex = &eastbound_mutex;
    } else if (train.direction == 'W') {
        queue = westbound_priority;
        queue_count = &westbound_priority_count;
        queue_mutex = &westbound_priority_mutex;
    } else if (train.direction == 'w') {
        queue = westbound;
        queue_count = &westbound_count;
        queue_mutex = &westbound_mutex;
    } else {
        printf("Invalid train: ");
        printf("%d, %c, %d, %d\n", train.train_no, train.direction, train.loading_time, train.crossing_time); 
        return;
    }

    // only one queue is locked at a time
    pthread_mutex_lock(queue_mutex);
    int insert_index = *queue_count;
    while (insert_index > 0 && queue[insert_index - 1].loading_time == train.loading_time &&
            queue[insert_index - 1].train_no > train.train_no) {
        insert_index--;
    }
    for (int i = *queue_count; i > insert_index; i--) {
    queue[i] = queue[i - 1];
    }
    queue[insert_index] = train;
    (*queue_count)++;

    if (train.direction == 'E' || train.direction == 'e') {
        printEastReady
    (train);
    }
    if (train.direction == 'W' || train.direction == 'w') {
        printWestReady(train);
    }
    pthread_mutex_unlock(queue_mutex);   
}

void cleanupQueues() {
    free(eastbound_priority);
    free(eastbound);
    free(westbound_priority);
    free(westbound);
    free(arrival_order);
}

void* trainThread(void* args){
    struct Train* train = (struct Train*) args;
    // Lock the mutex
    pthread_mutex_lock(&thread_count_mutex);
    thread_count++;
    // Wait for all threads to be created
    pthread_cond_wait(&thread_created_cond, &thread_count_mutex);
    // Unlock the mutex
    pthread_mutex_unlock(&thread_count_mutex);
    usleep(train->loading_time * 100000); // Sleep for loading_time * 0.1 seconds (convert to microseconds)
    enqueue(*train);
    return NULL;
}

void printMainTrack(struct Train train, struct Train* queue, int* queue_count){
    
    pthread_mutex_lock(&print_mutex);
    printTimeDifference();
    if (train.direction == 'E'|| train.direction == 'e'){
        printf("Train %2d is ON the main track going East\n", train.train_no);}
    else{
        printf("Train %2d is ON the main track going West\n", train.train_no);}
    pthread_mutex_unlock(&print_mutex);

    usleep(train.crossing_time * 100000); // Sleep for crossing_time * 0.1 seconds (convert to microseconds)
    
    pthread_mutex_lock(&print_mutex);
    printTimeDifference();
    if (train.direction == 'E'|| train.direction == 'e'){
        printf("Train %2d is OFF the main track after going East\n", train.train_no);}
    else{
        printf("Train %2d is OFF the main track after going West\n", train.train_no);}
    pthread_mutex_unlock(&print_mutex);
    
    //add to arrival queue
    addToArrivalQueue(train);
    
    // remove train from queue
    for (int i = 0; i < (*queue_count) - 1; i++) {
        queue[i] = queue[i + 1];
    }
    (*queue_count)--;
        
}

void* controllerThread(void* args){
    // only one controller thread is managing trains to get on the main track, rule 1 meeted
    // since we are assigning trains from ready queues, rule 2 meeted

    while (arrival_count < total_trains) {
        // Check if there are any high priority trains according to rule 3
        if (eastbound_priority_count > 0 || westbound_priority_count > 0){
            // if both priority queues are not empty, opposite direction rule(4b) applies
            if (eastbound_priority_count > 0 && westbound_priority_count > 0){
                // let westbound go first if no train has crossed the main track
                if (arrival_count > 0 && (arrival_order[arrival_count-1].direction == 'W'|| arrival_order[arrival_count-1].direction == 'w')){
                    printMainTrack(eastbound_priority[0], eastbound_priority, &eastbound_priority_count);
                }
                else{
                    printMainTrack(westbound_priority[0], westbound_priority, &westbound_priority_count);
                }
            }
            else if (eastbound_priority_count > 0 && westbound_priority_count == 0){
                if (arrival_count >= 3){//starvation prevention
                    if ((arrival_order[arrival_count-1].direction == 'E'|| arrival_order[arrival_count-1].direction == 'e') &&
                        (arrival_order[arrival_count-2].direction == 'E'|| arrival_order[arrival_count-2].direction == 'e') &&
                        (arrival_order[arrival_count-3].direction == 'E'|| arrival_order[arrival_count-3].direction == 'e') &&
                        westbound_count > 0){
                        printMainTrack(westbound[0], westbound, &westbound_count);
                    }
                    else{
                        printMainTrack(eastbound_priority[0], eastbound_priority, &eastbound_priority_count);
                    }
                }
                else{
                    printMainTrack(eastbound_priority[0], eastbound_priority, &eastbound_priority_count);
                }
            }
            else if (eastbound_priority_count == 0 && westbound_priority_count > 0){
                if (arrival_count >= 3){//starvation prevention
                    if ((arrival_order[arrival_count-1].direction == 'W'|| arrival_order[arrival_count-1].direction == 'w') &&
                        (arrival_order[arrival_count-2].direction == 'W'|| arrival_order[arrival_count-2].direction == 'w') &&
                        (arrival_order[arrival_count-3].direction == 'W'|| arrival_order[arrival_count-3].direction == 'w') &&
                        eastbound_count > 0){
                        printMainTrack(eastbound[0], eastbound, &eastbound_count);
                    }
                    else{
                        printMainTrack(westbound_priority[0], westbound_priority, &westbound_priority_count);
                    }
                }
                else{
                    printMainTrack(westbound_priority[0], westbound_priority, &westbound_priority_count);
                }
            }
        }

        // when priority queue is empty
        else{
            // if both ready queues are not empty, opposite direction rule(4b) applies
            if (eastbound_count > 0 && westbound_count > 0){
                // check last train that crossed the main track, let opposite direction train go first
                if (arrival_count > 0 && (arrival_order[arrival_count-1].direction == 'W'|| 
                    arrival_order[arrival_count-1].direction == 'w')){
                    printMainTrack(eastbound[0], eastbound, &eastbound_count);
                }
                else{
                    printMainTrack(westbound[0], westbound, &westbound_count);
                }
            }
            else if (eastbound_count > 0 && westbound_count == 0){
                printMainTrack(eastbound[0], eastbound, &eastbound_count);
            }
            else if (eastbound_count == 0 && westbound_count > 0){
                printMainTrack(westbound[0], westbound, &westbound_count);
            }
        } 
    }
    return NULL;
}   

int main(int argc, char* argv[]){
    pthread_mutex_init(&thread_count_mutex, NULL);
    pthread_cond_init(&thread_created_cond, NULL);
    FILE* fp;
    fp = fopen(argv[1], "r");
    if (fp == NULL) {
        printf("Error opening file\n");
        exit(1);
    }
    total_trains = countLines(fp);//start from 1
    struct Train train_data[total_trains]; // Create an array to store train data
    initializeQueues(total_trains);
    int train_counter = 0;

    // Read train data into the array
    while (train_counter < total_trains) {
        int input = (fscanf(fp, " %c %d %d", &train_data[train_counter].direction, 
                                           &train_data[train_counter].loading_time,
                                           &train_data[train_counter].crossing_time)==3);
        train_data[train_counter].train_no = train_counter;
        train_counter++;
    }
    pthread_t train_threads[total_trains];

    // Create threads using the train data from the array
    for (int i = 0; i < total_trains; i++) {
        pthread_create(&train_threads[i], NULL, trainThread, (void*)&train_data[i]);
    }

    while (1)
    {// Check if all threads have been created
        if (thread_count == total_trains) {
            // Signal the condition variable
            pthread_cond_broadcast(&thread_created_cond);
            setStartTime();
            break;
        }
    }
    pthread_t controller_thread;
    pthread_create(&controller_thread, NULL, controllerThread, NULL);
    pthread_join(controller_thread, NULL);

    // Wait for threads to finish
    for (int i = 0; i < total_trains; i++) {
        pthread_join(train_threads[i], NULL);
    }

    // Cleanup after arrival queue is full, prevent memory management error
    if (arrival_count == total_trains){
        cleanupQueues();
        pthread_mutex_destroy(&thread_count_mutex);
        pthread_cond_destroy(&thread_created_cond);
        pthread_mutex_destroy(&eastbound_priority_mutex);
        pthread_mutex_destroy(&eastbound_mutex);
        pthread_mutex_destroy(&westbound_priority_mutex);
        pthread_mutex_destroy(&westbound_mutex);
        pthread_mutex_destroy(&print_mutex);
    }
    
    fclose(fp);
    return 0;
}