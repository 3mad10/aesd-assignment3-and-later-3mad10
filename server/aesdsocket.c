

#include "aesdsocket.h"
#include "server_cfg.h"


bool g_server_running = true;

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
    g_server_running = false;
}

int main(int argc, char* argv[]) {
    int sfd, cfd;
    int err;
    struct sigaction termination_action;
    char client_addr[INET6_ADDRSTRLEN];
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

    while(g_server_running) {
        DEBUG_LOG("g_server_running = %d" , g_server_running);
        cfd = wait_for_connection(sfd, client_addr, sizeof(client_addr));

        if (cfd == -1) {
            // DEBUG_LOG("Connection Failed try to reconnect...");
            // syslog(LOG_ERR ,"Connection Failed try to reconnect...");
            break;
        }
        err = echo_conn(cfd);
        if (err == -1) {
            DEBUG_LOG("Connection failed with error %s", strerror(errno));
            syslog(LOG_ERR ,"Connection failed with error %s", strerror(errno));
        }

        close(cfd);

        printf("[Server] Closed connection from %s\n", client_addr);
        syslog(LOG_INFO ,"Closed connection from %s", client_addr);
    }
    syslog(LOG_INFO ,"Caught signal, exiting");
    DEBUG_LOG("Caught signal, exiting");

    err = shutdown(sfd, SHUT_RDWR);
    if (err != 0) {
        DEBUG_LOG("Failed to shutdown socket sfd = %d with error : %s", sfd, strerror(errno));
        syslog(LOG_ERR ,"Failed to shutdown socket sfd = %d with error : %s", sfd, strerror(errno));
    }
    err = shutdown(cfd, SHUT_RDWR);
    if (errno == EBADF) {
        DEBUG_LOG("Connection file already closed cfd");
        syslog(LOG_ERR ,"Connection file already closed");
    }
    if ((err != 0) && errno != EBADF) {
        DEBUG_LOG("Failed to shutdown socket cfd = %d with error : %s", cfd, strerror(errno));
        syslog(LOG_ERR ,"Failed to shutdown socket cfd = %d with error : %s", cfd, strerror(errno));
    }
    err = remove(RECEIVED_SOCKET_DATA_PATH);
    if (err != 0) {
        DEBUG_LOG("Failed to remove file %s", RECEIVED_SOCKET_DATA_PATH);
        syslog(LOG_ERR ,"Failed to remove file %s", RECEIVED_SOCKET_DATA_PATH);
    }

    return 0;
}

