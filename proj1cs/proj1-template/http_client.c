/*
 * CS 1652 Project 1 
 * (c) Jack Lange, 2020
 * (c) Amy Babay, 2022
 * (c) Samika Sanghvi, Aleksandar Smith
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
 
 #include <netdb.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <sys/socket.h>
 #include <sys/select.h>
 #include <errno.h>
 
 #define BUFSIZE 1024
 
 int 
 main(int argc, char ** argv) 
 {
 
     char * server_name = NULL;
     int    server_port = -1;
     char * server_path = NULL;
     char * req_str     = NULL;
 
     int ret = 0;
 
     /*parse args */
     if (argc != 4) {
         fprintf(stderr, "usage: http_client <hostname> <port> <path>\n");
         exit(-1);
     }
 
     server_name = argv[1];
     server_port = atoi(argv[2]);
     server_path = argv[3];
     
     /* Create HTTP request */
     ret = asprintf(&req_str, "GET %s HTTP/1.0\r\n\r\n", server_path);
     if (ret == -1) {
         fprintf(stderr, "Failed to allocate request string\n");
         exit(-1);
     }
 
     /*
      * NULL accesses to avoid compiler warnings about unused variables
      * You should delete the following lines 
      */
    //  (void)server_name;
    //  (void)server_port;
 
     /* make socket */
     int client_sock = socket(AF_INET, SOCK_STREAM, 0);
     if (client_sock < 0) {
         perror("socket");
         exit(-1);
     }
 
     /* get host IP address  */
     struct hostent *server_he = gethostbyname(server_name);
     if (!server_he) {
         fprintf(stderr, "gethostbyname failed for %s\n", server_name);
         exit(-1);
     }
 
     /* set address */
     struct sockaddr_in server_addr;
     memset(&server_addr, 0, sizeof(server_addr));
     server_addr.sin_family = AF_INET;
     server_addr.sin_port = htons(server_port);
     memcpy(&server_addr.sin_addr, server_he->h_addr_list[0], server_he->h_length);
 
     /* connect to the server */
     if (connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
         perror("connect");
         close(client_sock);
         exit(-1);
     }
 
     /* send request message */
     int sent_bytes = 0;
     int total_sent = 0;
     int req_len = strlen(req_str);
     while(total_sent < req_len) {
         sent_bytes = send(client_sock, req_str + total_sent, req_len - total_sent, 0);
         if (sent_bytes < 0) {
             perror("send");
             close(client_sock);
             exit(-1);
         }
         total_sent += sent_bytes;
     }
 
     /* wait for response (i.e. wait until socket can be read) */
     fd_set read_set;
     FD_ZERO(&read_set);
     FD_SET(client_sock, &read_set);
     if (select(client_sock+1, &read_set, NULL, NULL, NULL) < 0) {
         perror("select");
         close(client_sock);
         exit(-1);
     }
 
     /* first read loop -- read headers */
     char buffer[BUFSIZE];
     char response_header[BUFSIZE*4];
     int header_len = 0;
     bool header_received = false;
     while(!header_received) {
         int n = recv(client_sock, buffer, BUFSIZE, 0);
         if (n < 0) {
             perror("recv");
             close(client_sock);
             exit(-1);
         } else if (n == 0) {
             break;
         }
         if (header_len + n < sizeof(response_header)) {
             memcpy(response_header + header_len, buffer, n);
             header_len += n;
             response_header[header_len] = '\0';
             char * header_end = strstr(response_header, "\r\n\r\n");
             if (header_end != NULL) {
                 header_received = true;
                 int header_total_len = (header_end - response_header) + 4;
                 int extra_bytes = header_len - header_total_len;
                 char status_line[BUFSIZE];
                 sscanf(response_header, "%[^\r\n]", status_line);
                 int http_major, http_minor, status_code;
                 char status_text[BUFSIZE];
                 if (sscanf(status_line, "HTTP/%d.%d %d %[^\r\n]", &http_major, &http_minor, &status_code, status_text) < 3) {
                     fprintf(stderr, "Incorect HTTP response\n");
                     close(client_sock);
                     exit(-1);
                 }
                 if (status_code == 200) {
                     if (extra_bytes > 0) {
                         fwrite(response_header + header_total_len, 1, extra_bytes, stdout);
                     }
                     /* second read loop -- print out the rest of the response: real web content */
                     while ( (n = recv(client_sock, buffer, BUFSIZE, 0)) > 0 ) {
                         fwrite(buffer, 1, n, stdout);
                     }
                     close(client_sock);
                     exit(0);
                 } else {
                     fprintf(stderr, "%s", response_header);
                     while ( (n = recv(client_sock, buffer, BUFSIZE, 0)) > 0 ) {
                         fwrite(buffer, 1, n, stderr);
                     }
                     close(client_sock);
                     exit(-1);
                 }
             }
         } else {
             fprintf(stderr, "Header too large\n");
             close(client_sock);
             exit(-1);
         }
     }
 
     /* second read loop -- print out the rest of the response: real web content */
     int n;
     while ((n = recv(client_sock, buffer, BUFSIZE, 0)) > 0) {
         fwrite(buffer, 1, n, stdout);
     }
 
     /* close socket */
     close(client_sock);
     free(req_str);
     return 0;
 }
 