
/*
 * CS 1652 Project 1 
 * (c) Jack Lange, 2020
 * (c) Samika Sanghvi, Aleksandar Smith
 * 
 * Computer Science Department
 * University of Pittsburgh
 */

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


#include "pet_list.h"
#include "pet_hashtable.h"


#define FILENAMESIZE 100
#define BUFSIZE      1024
#define MAX_CLIENTS FD_SETSIZE

#define STATE_NO_CONNECTION -1
#define STATE_RECEIVING 0
#define STATE_PARSING_REQUEST 1
#define STATE_READING_FILE 2
#define STATE_SENDING_HEADER 3
#define STATE_SENDING_BODY 4


/* Global connection tracking structure */
struct connection {
    int socket;
    char *request_buffer;
    FILE *file;
    int fd;
    int filesize;
    char *file_buffer;
    int state;
    int status_code;
};

void init_connection(struct connection *conn, int socket) {
    conn->socket = socket;
    conn->request_buffer = NULL;
    conn->file = NULL;
    conn->fd = -1;
    conn->filesize = -1;
    conn->file_buffer = NULL;
    conn->state = STATE_RECEIVING;
    conn->status_code = -1;
}

void clear_connection(struct connection *conn) {
    conn->state = STATE_NO_CONNECTION;
    if (conn->file != NULL) {
        fclose(conn->file);
        conn->file = NULL;
    }
    if (conn->request_buffer != NULL) {
        free(conn->request_buffer);
        conn->request_buffer = NULL;
    }
    if (conn->file_buffer != NULL) {
        free(conn->file_buffer);
        conn->file_buffer = NULL;
    }
}


/*
 * You are not required to use this function, but can use it or modify it as you see fit 
 */
static void 
send_response(struct connection * con) 
{
    char * ok_response_f  = "HTTP/1.0 200 OK\r\n"     					\
       					    "Content-type: text/plain\r\n"              \
        				    "Content-length: %d \r\n\r\n";
    
    char * notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"  			\
        					"Content-type: text/html\r\n\r\n"           \
        					"<html><body bgColor=black text=white>\n"   \
        					"<h2>404 FILE NOT FOUND</h2>\n"             \
        					"</body></html>\n";

    /* send headers */
    if (con->state == STATE_SENDING_HEADER) {
        if (con->status_code == 200) {
            char *ok_response;
            asprintf(&ok_response, ok_response_f, con->filesize);
            int send_res = send(con->socket, ok_response, strlen(ok_response), 0);
            if (send_res < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                free(ok_response);
                return; // try again later
            }
            free(ok_response);
        } else if (con->status_code == 404) {
            int send_res = send(con->socket, notok_response, strlen(notok_response), 0);
            if (send_res < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
                return; // try again later
            }
        }
        con->state = STATE_SENDING_BODY;
    }

    /* send response */
    if (con->state == STATE_SENDING_BODY) {
        int send_res = send(con->socket, con->file_buffer, con->filesize, 0);
        if (send_res < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            return; // try again later
        }       
    }

}

/*
 * You are not required to use this function, but can use it or modify it as you see fit 
 */
static void 
handle_file_data(struct connection * con) 
{
	/* Read available file data */
	int read_res = fread(con->file_buffer, 1, con->filesize, con->file);

	/* Check if we have read entire file */
	if (read_res < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
	    return; // try again when we have more data
	}

	/* If we have read the entire file, send response to client */
	con->state = STATE_SENDING_HEADER;

}


/*
 * You are not required to use this function, but can use it or modify it as you see fit 
 */
static void 
handle_request(struct connection * con)
{
    if (con->fd == -1) {
        /* parse request to get file name */
        char method[16], path[FILENAMESIZE], protocol[16];
        sscanf(con->request_buffer, "%15s %255s %15s", method, path, protocol);
        char *filename = (path[0] == '/') ? path + 1 : path;
        if (strlen(filename) == 0) filename = "index.html";

        /* Assumption: For this project you only need to handle GET requests and filenames that contain no spaces */

        /* try opening the file */
        FILE *f = fopen(filename, "rb");
        if (f == NULL) {
            con->status_code = 404;
            con->state = STATE_SENDING_HEADER;
            return;
        }
        con->file = f;
        con->fd = fileno(con->file);
        con->status_code = 200;
        
        /* get file size */
        struct stat filestats;
        fstat(con->fd, &filestats);
        con->filesize = filestats.st_size;
        
        /* set to non-blocking */
        fcntl(con->fd, F_SETFL, O_NONBLOCK);
        
    }

    /* Initiate non-blocking file read operations */
    con->file_buffer = malloc(con->filesize);
    con->state = STATE_READING_FILE;

}

/*
 * You are not required to use this function, but can use it or modify it as you see fit 
 */
static void 
handle_network_data(struct connection * con) 
{
	/* Read all available request data */
	if (con->request_buffer == NULL) {
	    con->request_buffer = malloc(BUFSIZE);
	}
	int recvd = recv(con->socket, con->request_buffer, BUFSIZE-1, 0);

        if (recvd < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
            return; // try again later
        }

	/* If we have the entire request, then handle the request */
	con->state = STATE_PARSING_REQUEST;
	handle_request(con);
	
}






int
main(int argc, char ** argv) 
{
    int server_port = -1;

    /* parse command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: http_server3 port\n");
        exit(-1);
    }

    server_port = atoi(argv[1]);

    if (server_port < 1500) {
        fprintf(stderr, "INVALID PORT NUMBER: %d; can't be < 1500\n", server_port);
        exit(-1);
    }
    
    /* Initialize connection tracking data structure */
    struct connection client_conns[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_conns[i].state = STATE_NO_CONNECTION;
    }

    /* initialize and make server socket */
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); exit(1); }
    fcntl(listenfd, F_SETFL, O_NONBLOCK);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /* set server address */
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(server_port) };

    /* bind listening socket */
    if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { 
        perror("bind");
        exit(1);
    }
    
    /* start listening */
    if (listen(listenfd, 10) < 0) {
        perror("listen");
        exit(1);
    }
    
    /* set up for connection handling loop */

    while (1) {

        /* create read and write lists */
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
	FD_ZERO(&writefds);
        FD_SET(listenfd, &readfds);
        int maxfd = listenfd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
	    int state = client_conns[i].state;
	    if (state == STATE_RECEIVING) {
	        FD_SET(client_conns[i].socket, &readfds);
	        if (client_conns[i].socket > maxfd)
	            maxfd = client_conns[i].socket;
	    } else if (state == STATE_READING_FILE) {
		FD_SET(client_conns[i].fd, &readfds);
		if (client_conns[i].fd > maxfd)
		    maxfd = client_conns[i].fd;
	    } else if (state >= STATE_SENDING_HEADER) {
		FD_SET(client_conns[i].socket, &writefds);
		if (client_conns[i].socket > maxfd)
		    maxfd = client_conns[i].socket;
	    }
	}
        
        /* do a select */
        if (select(maxfd + 1, &readfds, &writefds, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            perror("select"); break;
        }
        
        if (FD_ISSET(listenfd, &readfds)) {
            struct sockaddr_in cli_addr;
            socklen_t cli_len = sizeof(cli_addr);
            int newfd = accept(listenfd, (struct sockaddr *)&cli_addr, &cli_len);
            if (newfd < 0) {
                perror("accept"); 
                continue;
            }
	    fcntl(newfd, F_SETFL, O_NONBLOCK);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_conns[i].state == STATE_NO_CONNECTION) {
		    init_connection(&client_conns[i], newfd);
		    break;
		}
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {

	    int state = client_conns[i].state;

	    if (state == STATE_RECEIVING && FD_ISSET(client_conns[i].socket, &readfds)) {
	        handle_network_data(&client_conns[i]);
	    }
	    if (state == STATE_READING_FILE && FD_ISSET(client_conns[i].fd, &readfds)) {
	        handle_file_data(&client_conns[i]);
	    }
	    if (state == STATE_SENDING_HEADER && FD_ISSET(client_conns[i].socket, &writefds)) {
		send_response(&client_conns[i]);
		close(client_conns[i].socket);
		clear_connection(&client_conns[i]);
	    }

        }

    }
    
    close(listenfd);
    return 0;       

}

