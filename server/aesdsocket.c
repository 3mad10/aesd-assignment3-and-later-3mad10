

#include "aesdsocket.h"
#include "server_cfg.h"

struct slist_job_data
{
    struct job_data* elm;
    SLIST_ENTRY(slist_job_data) entries;
};

static volatile sig_atomic_t g_server_running = 1;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#if USE_AESD_CHAR_DEVICE != 1
static pthread_t g_timestamp_thread_id;
#endif

SLIST_HEAD(slisthead, slist_job_data) head = SLIST_HEAD_INITIALIZER(head);

static void sigint_handler (int signo)
{
    if (signo == SIGINT)
        DEBUG_LOG("Caught signal handler for SIGINT");
    else if (signo == SIGTERM)
        DEBUG_LOG("Caught signal handler for SIGTERM");
    else {
        /* this should never happen */
        exit (EXIT_FAILURE);
    }
    g_server_running = 0;
    
}


void* threadfunc(void* data)
{
    int err;
    struct job_data * job_data = (struct job_data *) data;
    job_data->job_state = PENDING;
    err = echo_conn(job_data);
    if (err == -1) {
        job_data->job_state = FAILED;
        DEBUG_LOG("Connection failed with error %s", strerror(errno));
        syslog(LOG_ERR ,"Connection failed with error %s", strerror(errno));
    } else {
        job_data->job_state = SUCCESS;
    }

    close(job_data->cfd);

    printf("[Server] Closed connection from %s\n", job_data->client);
    syslog(LOG_INFO ,"Closed connection from %s", job_data->client);
    return (void*)job_data;
}

#if USE_AESD_CHAR_DEVICE != 1
void* timestamp_threadfunc(void* data)
{
    int fd;
    char outstr[200];
    time_t t;
    struct tm *tmp;
    int len_timestamp;
    DEBUG_LOG("Entered timestamp thread");
    while(g_server_running) {
        t = time(NULL);
        tmp = localtime(&t);
        len_timestamp = strftime(outstr, sizeof(outstr), "timestamp:%a, %d %b %Y %T %z %n", tmp);
        fd = open(RECEIVED_SOCKET_DATA_PATH, O_CREAT | O_WRONLY | O_APPEND, 0664);
        if (fd==-1) {
            syslog(LOG_ERR, "Could not open file %s", RECEIVED_SOCKET_DATA_PATH);
            DEBUG_LOG("Could not open file %s", RECEIVED_SOCKET_DATA_PATH);
        }
        if (len_timestamp == 0) {
            syslog(LOG_ERR, "Failed to format timestamp");
            DEBUG_LOG("Failed to format timestamp");
        } else {

            pthread_mutex_lock(&mutex);
            write_to_file(fd, &outstr[0], len_timestamp);
            pthread_mutex_unlock(&mutex);
        }
        close(fd);
        for (int i = 0; (i < TIMESTAMP_THREAD_SLEEP_TIME) && (g_server_running); i++) {
            sleep(1);
        }
    }
    (void)data;
    return NULL;
}
#endif

int create_job_thread(int cfd, char* client_addr) {
    int err;
    pthread_t thread;
    struct job_data* job_data = malloc(sizeof(struct job_data));
    job_data->mutex = &mutex;
    job_data->cfd = cfd;
    job_data->job_state = NOT_STARTED;
    memcpy(job_data->client, client_addr, INET6_ADDRSTRLEN);
    err = pthread_create(&thread, NULL, threadfunc, (void*)job_data);
    if(err == 0) {
        struct slist_job_data * slist_job_data = malloc(sizeof(struct slist_job_data));
        job_data->thread_id = thread;
        slist_job_data->elm = job_data;
        SLIST_INSERT_HEAD(&head, slist_job_data, entries);
    } else {
        DEBUG_LOG("Failed to create thread with error %s", strerror(errno));
        syslog(LOG_ERR ,"Failed to create thread with error %s", strerror(errno));
        free(job_data);
    }
    return err;
}

int main(int argc, char* argv[]) {
    int sfd, cfd;
    int err;
    struct sigaction termination_action;
    char client_addr[INET6_ADDRSTRLEN];
    struct slist_job_data* c;
    struct slist_job_data* n;
    #if USE_AESD_CHAR_DEVICE !=1
    pthread_t timestamp_thread;
    #endif
    bool run_as_daemon = false;
    
    if (argc > 2) {
        DEBUG_LOG("Unsupported nuber of \
            arguents only supported argument is -d");
            return -1;
        }
    if (argc == 2) {
        if ((strcmp(argv[1], "-d") == 0)) {
            run_as_daemon = true;
        } else {
            DEBUG_LOG("Supported options are : [-d]");
        }
    }
    
    memset(&termination_action, 0, sizeof(termination_action));
    termination_action.sa_handler=sigint_handler;

    err = sigaction(SIGTERM, &termination_action, NULL);
    if (err != 0) {
        DEBUG_LOG("Failed to set signal handler for SIGTERM");
        exit (EXIT_FAILURE);
    }

    err = sigaction(SIGINT, &termination_action, NULL);
    if (err != 0) {
        DEBUG_LOG("Failed to set signal handler for SIGINT");
        exit (EXIT_FAILURE);
    }

    sfd = setup_socket(SERVER_PORT, SOCK_STREAM);
    if (sfd == -1) {
        DEBUG_LOG("Failed to Setup Socket with error %s", strerror(errno));
        syslog(LOG_ERR ,"Failed to Setup Socket with error %s", strerror(errno));
        exit (EXIT_FAILURE);
    }

    err = listen(sfd, BACKLOG); 
    if (err == -1) {
        close(sfd);
        DEBUG_LOG("Listen failed with error %s", strerror(errno));
        syslog(LOG_ERR ,"Listen failed with error %s", strerror(errno));
        exit (EXIT_FAILURE);
    }
    
    printf("[Server] waiting for connections...\n");
    if (run_as_daemon) {
        daemon(0, 0);
    }
    #if USE_AESD_CHAR_DEVICE != 1
    pthread_create(&timestamp_thread, NULL, timestamp_threadfunc, NULL);
    g_timestamp_thread_id = timestamp_thread;
    #endif
    while(g_server_running) {
        cfd = wait_for_connection(sfd, client_addr, sizeof(client_addr));

        if (cfd == -1) {
            // DEBUG_LOG("Connection Failed try to reconnect...");
            // syslog(LOG_ERR ,"Connection Failed try to reconnect...");
            break;
        }
        create_job_thread(cfd, client_addr);
        SLIST_FOREACH_SAFE(c, &head, entries, n){
            if (c->elm->job_state == SUCCESS || c->elm->job_state == FAILED) {
                pthread_join(c->elm->thread_id, NULL);
                free(c->elm);
                SLIST_REMOVE(&head, c, slist_job_data, entries);
                free(c);
            }
        }
    }
    syslog(LOG_INFO ,"Caught signal, exiting");
    DEBUG_LOG("Caught signal, exiting");

    SLIST_FOREACH_SAFE(c, &head, entries, n){
        pthread_join(c->elm->thread_id, NULL);
        err = shutdown(c->elm->cfd, SHUT_RDWR);
        if (errno == EBADF) {
            DEBUG_LOG("Connection file already closed cfd");
            syslog(LOG_ERR ,"Connection file already closed");
        }
        if ((err != 0) && errno != EBADF) {
            DEBUG_LOG("Failed to shutdown socket cfd = %d with error : %s", c->elm->cfd, strerror(errno));
            syslog(LOG_ERR ,"Failed to shutdown socket cfd = %d with error : %s", c->elm->cfd, strerror(errno));
        }
        free(c->elm);
        SLIST_REMOVE(&head, c, slist_job_data, entries);
        free(c);
    }

    #if USE_AESD_CHAR_DEVICE != 1
    pthread_join(g_timestamp_thread_id, NULL);
    #endif

    err = shutdown(sfd, SHUT_RDWR);
    if (err != 0) {
        DEBUG_LOG("Failed to shutdown socket sfd = %d with error : %s", sfd, strerror(errno));
        syslog(LOG_ERR ,"Failed to shutdown socket sfd = %d with error : %s", sfd, strerror(errno));
    }
    DEBUG_LOG("AFTER SHUTDOWN");
    #if USE_AESD_CHAR_DEVICE != 1
    err = remove(RECEIVED_SOCKET_DATA_PATH);
    if (err != 0) {
        DEBUG_LOG("Failed to remove file %s", RECEIVED_SOCKET_DATA_PATH);
        syslog(LOG_ERR ,"Failed to remove file %s", RECEIVED_SOCKET_DATA_PATH);
    }
    #endif
    DEBUG_LOG("AFTER REMOVE FILE");
    return 0;
}

