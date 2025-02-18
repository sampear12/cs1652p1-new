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
 #include <errno.h>
 
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <sys/socket.h>
 #include <sys/stat.h>
 #include <sys/select.h>
 
 #define BUFSIZE 1024
 #define FILENAMESIZE 100
 #define MAX_CONNECTIONS  FD_SETSIZE
 
 static int handle_connection(int conn_sock) 
 {
     char * ok_response_f  = "HTTP/1.0 200 OK\r\n"						\
                             "Content-type: text/plain\r\n"				\
                             "Content-length: %d \r\n\r\n";
     
     char * notok_response = "HTTP/1.0 404 FILE NOT FOUND\r\n"			\
                             "Content-type: text/html\r\n\r\n"			\
                             "<html><body bgColor=black text=white>\n"	\
                             "<h2>404 FILE NOT FOUND</h2>\n"				\
                             "</body></html>\n";
 
     /* first read loop -- get request and headers*/
     char req_buffer[BUFSIZE];
     int bytes_read = recv(conn_sock, req_buffer, BUFSIZE-1, 0);
     if (bytes_read <= 0) {
         close(conn_sock);
         return -1;
     }
     req_buffer[bytes_read] = '\0';
 
     /* parse request to get file name */
     char method[10], file_path[FILENAMESIZE], http_version[10];
     if (sscanf(req_buffer, "%s %s %s", method, file_path, http_version) != 3) {
         close(conn_sock);
         return -1;
     }
 
     if (strcmp(method, "GET") != 0) {
         close(conn_sock);
         return -1;
     }
     
     /* open and read the file */
     FILE *fp = fopen(file_path + 1, "rb"); // skip the leading '/'
     if (!fp) {
         send(conn_sock, notok_response, strlen(notok_response), 0);
         close(conn_sock);
         return -1;
     }
     
     struct stat st;
     if (stat(file_path + 1, &st) < 0) {
         fclose(fp);
         close(conn_sock);
         return -1;
     }
     int file_size = st.st_size;
 
     /* send response */
     char header_buffer[BUFSIZE];
     int header_len = snprintf(header_buffer, BUFSIZE, ok_response_f, file_size);
     send(conn_sock, header_buffer, header_len, 0);
 
     char file_buffer[BUFSIZE];
     int n;
     while ((n = fread(file_buffer, 1, BUFSIZE, fp)) > 0) {
         send(conn_sock, file_buffer, n, 0);
     }
     
     fclose(fp);
     close(conn_sock);
     return 0;
 }
 
 
 
 int
 main(int argc, char ** argv)
 {
     int server_port = -1;
     int ret         =  0;
     int listen_sock = -1;
     int client_socks[MAX_CONNECTIONS];
     int num_clients = 0;
     fd_set read_fds;
     int max_fd;
     int i;
 
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
     listen_sock = socket(AF_INET, SOCK_STREAM, 0);
     if (listen_sock < 0) {
         perror("socket");
         exit(-1);
     }
     int opt = 1;
     setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
 
     /* set server address */
     struct sockaddr_in server_addr;
     memset(&server_addr, 0, sizeof(server_addr));
     server_addr.sin_family = AF_INET;
     server_addr.sin_addr.s_addr = INADDR_ANY;
     server_addr.sin_port = htons(server_port);
 
     /* bind listening socket */
     if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
         perror("bind");
         close(listen_sock);
         exit(-1);
     }
 
     /* start listening */
     if (listen(listen_sock, 10) < 0) {
         perror("listen");
         close(listen_sock);
         exit(-1);
     }
 
     /* connection handling loop: wait to accept connection */
     while (1) {
         FD_ZERO(&read_fds);
         FD_SET(listen_sock, &read_fds);
         max_fd = listen_sock;
 
         // Add client sockets to read_fds
         for (i = 0; i < num_clients; i++) {
             FD_SET(client_socks[i], &read_fds);
             if (client_socks[i] > max_fd)
                 max_fd = client_socks[i];
         }
 
         int sel_ret = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
         if (sel_ret < 0) {
             perror("select");
             continue;
         }
 
         // Check if new connection is incoming
         if (FD_ISSET(listen_sock, &read_fds)) {
             struct sockaddr_in client_addr;
             socklen_t client_len = sizeof(client_addr);
             int new_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
             if (new_sock >= 0) {
                 if (num_clients < MAX_CONNECTIONS) {
                     client_socks[num_clients++] = new_sock;
                 } else {
                     close(new_sock);
                 }
             }
         }
 
         // Check each client socket
         for (i = 0; i < num_clients; ) {
             if (FD_ISSET(client_socks[i], &read_fds)) {
                 ret = handle_connection(client_socks[i]);
                 // Remove this socket from the list by shifting others
                 for (int j = i; j < num_clients - 1; j++) {
                     client_socks[j] = client_socks[j+1];
                 }
                 num_clients--;
             } else {
                 i++;
             }
         }
     }
     close(listen_sock);
     return 0;
 }
 