/*
 * CS 1652 Project 1 
 * (c) Jack Lange, 2020
 * (c) Amy Babay, 2022
 * (c) <Student names here>
 * 
 * Computer Science Department
 * University of Pittsburgh
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFSIZE 1024
#define FILENAMESIZE 100


static int 
handle_connection(int sock) 
{
 
    char * ok_response_f  = "HTTP/1.0 200 OK\r\n"						\
                            "Content-type: text/plain\r\n"				\
                            "Content-length: %d \r\n\r\n";
    
    char * notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"			\
                            "Content-type: text/html\r\n\r\n"			\
                            "<html><body bgColor=black text=white>\n"	\
                            "<h2>404 FILE NOT FOUND</h2>\n"				\
                            "</body></html>\n";
    
  (void)notok_response;  // DELETE ME
  (void)ok_response_f;   // DELETE ME

    /* first read loop -- get request and headers*/

    /* parse request to get file name */

    /* Assumption: For this project you only need to handle GET requests and filenames that contain no spaces */
  
    /* open and read the file */
  
    /* send response */

    /* close socket and free space */
  
    return 0;
}



int
main(int argc, char ** argv)
{
    int server_port = -1;
    int ret         =  0;
    int sock        = -1;

    /* parse command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: http_server2 port\n");
        exit(-1);
    }

    server_port = atoi(argv[1]);

    if (server_port < 1500) {
        fprintf(stderr, "Requested port(%d) must be above 1500\n", server_port);
        exit(-1);
    }
    
    /* initialize and make socket */

    /* set server address */

    /* bind listening socket */

    /* start listening */

    /* connection handling loop: wait to accept connection */

    while (1) {
    
        /* create read list */
        
        /* do a select() */
        
        /* process sockets that are ready:
         *     for the accept socket, add accepted connection to connections
         *     for a connection socket, handle the connection
         */
        
        ret = handle_connection(sock);
        
        (void)ret;  // DELETE ME
    }
}
