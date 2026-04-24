#include "socket_handling.h"

static void *get_in_addr(struct sockaddr *sa);
static int send_received_data(int cfd);
static void save_received_data(const char* recv_buff, int n);
static void send_chunk(int cfd, const char* buf, int len); 

int setup_socket(int port, int sock_type) {
    int err;
    int sfd;
    struct addrinfo hints, *res;
    char port_str[20];
    int opts = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = sock_type;
    hints.ai_flags = AI_PASSIVE; 

    sprintf(port_str, "%d", port);

    err = getaddrinfo(NULL, port_str, &hints, &res);
    if (err != 0) {
        DEBUG_LOG("Failed to get address with errno %s", gai_strerror(err));
        exit (EXIT_FAILURE);
    }

    sfd = socket(res->ai_family, res->ai_socktype,res->ai_protocol);
    if (sfd == -1){
        DEBUG_LOG("Socket creation failed with error %s", strerror(errno));
        exit (EXIT_FAILURE);
    }

	err = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(int));
	if (err == -1) 
	{
		DEBUG_LOG("Error in setsockopt");
        syslog(LOG_ERR ,"Setting Socket Options failed with error %s", strerror(errno));

	}

    err = bind(sfd, res->ai_addr, res->ai_addrlen);
    if (err != 0) {
        close(sfd);
        DEBUG_LOG("Failed to bind: %s", gai_strerror(err));
        freeaddrinfo(res);
        exit (EXIT_FAILURE);
    }

    freeaddrinfo(res);

    return sfd;
}

int wait_for_connection(int sfd, char* client_addr, int addr_len) {
    int cfd;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_size;

    peer_addr_size = sizeof(peer_addr);
    cfd = accept(sfd, (struct sockaddr *)&peer_addr, &peer_addr_size);
    if (cfd == -1) {
        if (errno != EINTR) {
            DEBUG_LOG("Accept failed with error %s", strerror(errno));
            syslog(LOG_ERR ,"Accept failed with error %s", strerror(errno));
        }
        return -1;
    }

    inet_ntop(peer_addr.ss_family,
        get_in_addr((struct sockaddr *)&peer_addr),
        client_addr, addr_len);
    printf("[Server] got connection from %s\n", client_addr);
    syslog(LOG_INFO ,"Accepted connection from %s", client_addr);

    return cfd;
}


int echo_conn(int cfd) {
    int n;
    char* recv_buff = (char *)malloc(RCV_CHUNK_SIZE);
    long unsigned int total_size = 0;
    long unsigned int capacity = RCV_CHUNK_SIZE;
    DEBUG_LOG("allocated buffer with size %d", capacity);
    if (recv_buff == NULL) return -1;

    while(((n = recv(cfd, &recv_buff[total_size], capacity-total_size, 0))!=0)) {
        DEBUG_LOG("received %d bytes", n);
        if (n == -1) {
            if (errno == EINTR) continue;
            DEBUG_LOG("Error receiving data");
            break;
        }
        total_size += n;
        if (memchr(&recv_buff[total_size - n], '\n', n) != NULL) {
            DEBUG_LOG("saving recv_buff byte[0] = %c to byte[%ld] = %c", recv_buff[0], total_size, recv_buff[total_size - 2]);
            save_received_data((char*)&recv_buff[0], total_size);
            send_received_data(cfd);
            total_size = 0;
        }
        if (total_size > capacity - REALLOC_LIMIT) {
            DEBUG_LOG("Reallocating buffer");
            recv_buff = (char *)realloc(recv_buff,
                capacity + RCV_CHUNK_SIZE);
            capacity += RCV_CHUNK_SIZE;
            if (recv_buff == NULL) {
                DEBUG_LOG("Error reallocating receive buffer");
                return -1;
            }
            DEBUG_LOG("New buffer size = %d byte", capacity);
        }
    }
    free(recv_buff);
    
    if(n == -1) {
        return -1;
    }

    return 0;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    } else {
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
}


int send_received_data(int cfd) {
    int n;
    int fd;
    char transmit_buff[TRN_BUFF_SIZE];
    DEBUG_LOG("Sending data to client");
    fd = open(RECEIVED_SOCKET_DATA_PATH, O_RDONLY);
    while ((n = read (fd, &transmit_buff[0], sizeof(transmit_buff))) > 0) {
        DEBUG_LOG("n = %d" , n);
        DEBUG_LOG("char[0] = %c , char[n-2] = %c" , transmit_buff[0], transmit_buff[n-2]);
        if (n == -1) {
            syslog(LOG_ERR, "Error Sending Chunk to client");
            DEBUG_LOG("send failed with error", strerror(errno));
        }
        send_chunk(cfd, transmit_buff, n);
    }
    close(fd);
    return 0;
}

void send_chunk(int cfd, const char* buf, int len) {
    int n;
    DEBUG_LOG("Send chunk with params fd = %d len = %d" ,cfd , len);
    DEBUG_LOG("First char = %c" ,buf[0]);
    do
    {
        n = send(cfd, buf, len, 0);
        if (n == -1) {
            syslog(LOG_ERR, "Error Sending Chunk to client");
            DEBUG_LOG("send failed with error", strerror(errno));
            break;
        }
        DEBUG_LOG("n = %d", n);
        len = len - n;
    } while(len > 0);

}


void save_received_data(const char* recv_buff, int n) {
    int fd;

    fd = open(RECEIVED_SOCKET_DATA_PATH, O_CREAT | O_WRONLY | O_APPEND, 0664);
    if (fd==-1) {
        syslog(LOG_ERR, "Could not open file %s", RECEIVED_SOCKET_DATA_PATH);
        DEBUG_LOG("Could not open file %s", RECEIVED_SOCKET_DATA_PATH);
    }
    DEBUG_LOG("Created file with fd = %d", fd);
    write_to_file(fd, &recv_buff[0], n);
    close(fd);
}

