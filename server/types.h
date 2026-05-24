#ifndef TYPES_H
#define TYPES_H

#include <netinet/in.h>
#include <stdbool.h>
#include <pthread.h>


typedef enum{
    SUCCESS = 0,
    FAILED = 1,
    PENDING = 2,
    NOT_STARTED = 3
} Job_State;

struct job_data{
    pthread_mutex_t* mutex;
    pthread_t thread_id;
    char client[INET6_ADDRSTRLEN];
    Job_State job_state;
    int cfd;
    /**
     * Set to true if the thread completed with success, false
     * if an error occurred.
     */
    bool thread_complete_success;
};

#endif // TYPES_H