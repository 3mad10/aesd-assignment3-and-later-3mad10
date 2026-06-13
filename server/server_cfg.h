#ifndef SERVER_CFG_H
#define SERVER_CFG_H

#define USE_AESD_CHAR_DEVICE 1
#define DEBUG_LOG(msg,...) printf("[DEBUG] " msg "\n" , ##__VA_ARGS__)
#define RCV_CHUNK_SIZE 1024
#define TRN_BUFF_SIZE 1024
#define SERVER_PORT 9000  // the port users will be connecting to
#define BACKLOG 10
#if USE_AESD_CHAR_DEVICE == 1
#define RECEIVED_SOCKET_DATA_PATH "/dev/aesdchar"
#else 
#define RECEIVED_SOCKET_DATA_PATH "/var/tmp/aesdsocketdata"
#endif
#define REALLOC_LIMIT 2
#if USE_AESD_CHAR_DEVICE != 1
#define TIMESTAMP_THREAD_SLEEP_TIME 10
#endif
#endif // SERVER_CFG
