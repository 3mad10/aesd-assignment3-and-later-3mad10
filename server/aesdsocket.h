#ifndef AESDSOCKET_H
#define AESDSOCKET_H

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

#include "queue.h"
#include "file_handling.h"
#include "socket_handling.h"
#include "types.h"

int send_received_data(int cfd);
void send_chunk(int cfd, const char* buf, int len);
int handle_conn(int cfd);
int start_job_thread(int cfd);

#endif // AESDSOCKET_H
