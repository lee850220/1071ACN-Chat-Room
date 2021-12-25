/* fork listening child */
    if (pipe(pipe_fd) == -1) { // use pipe for parent & child connection
        perror("-Error");
        return 0;
    }

Server_MInfo.listen_pid = fork();										
    if (Server_MInfo.listen_pid > 0) { 

        // In Parent
        close(pipe_fd[1]); // close write fd   
           
        while(1) {

            /* store client fd */
            read(pipe_fd[0], &client_socket_fd, sizeof(client_socket_fd)); // read new client fd from child
            empty_index = search_empty_socket();
            if (empty_index != -1) {
                Server_MInfo.c_socket_arr[empty_index] = client_socket_fd;
            }

            /* transmission data between clients */
            for (i = 0; i < NUM_CLIENT_QUEUE; i++) {
                if (Server_MInfo.c_socket_arr[i] != -1) {
                    printf("clientID=%d\n", client_socket_fd);
                    send(client_socket_fd, SUCC_CONNECTION, sizeof(SUCC_CONNECTION), SEND_NORMAL);
                    recv(client_socket_fd, buf, sizeof(buf), RECV_NORMAL);
                    printf("[RECV]ClientID=%d: %s\n", client_socket_fd, buf);
                }
            }
            
        }

    } else if (Server_MInfo.listen_pid == 0) { 

        // In child
        close(pipe_fd[0]); // close read fd


        /* listening socket */
        while (1) {

            client_socket_fd = accept(server_socket_fd, (struct sockaddr *) &client_info, (socklen_t *) &addrlen);
            write(pipe_fd[1], &client_socket_fd, sizeof(client_socket_fd));
            //printf("fd=%d\nindex=%d\n%d\n", client_socket_fd, empty_index, Server_MInfo.c_socket_arr[empty_index]);
        }
        
    } else {
        perror("-Error");
    }