/*     Dependent Library     */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*     Definition Parameters     */
#define DEFAULT_PORT            12345
#define NUM_CLIENT_QUEUE        5
#define BUF_SIZE                1000
#define NAME_LENGTH             1000
#define SEND_NORMAL             0
#define RECV_NORMAL             0
#define SUCC_EXIT               "BYE!\n"
#define SUCC_JOIN               "[System]: ClientFD=%d: %s [%s]\n"
#define SUCC_ADD_CLIENT         "[System]: Adding client on fd "
#define SUCC_REM_CLIENT         "[System]: Removing client on fd "
#define SUCC_START_LISTEN       "[System]: Waiting for client connect...\n"
#define SUCC_RECV_LOGIN         " is join KChat!"
#define ERR_SOCKET_CREATE       "[System]: Fail to create a socket.\n"
#define ERR_CONNECTION          "[System]: Connection error.\n"
#define ERR_OUT_OF_MEMORY       "[System]: Server out of memory.\n"
#define SEND_SUCC_CONNECTION    "Login successfully"


/*     Data Structure     */
struct Fd_User {                                // username index by file description
    char name[NAME_LENGTH];
};

struct UserInfo {                               // linked list userinfo block
    char name[NAME_LENGTH];
    char room[NAME_LENGTH];
    struct UserInfo * prev;
    struct UserInfo * next;
};

struct Chatroom {                               // chatroom info block
    int num_user;
    char roomname[NAME_LENGTH];
    char user[NUM_CLIENT_QUEUE][NAME_LENGTH];   // an array of user name (char array)
};

struct ServerManageInfo {                       // server manage info (SMI) block
    int fd_size;
    int num_user;
    struct UserInfo * user;                         // a pointer of UserInfo (dynamic allocate)
    struct Fd_User * fd_user;                       // a pointer of username (index by fd) (dynamic allocate)
    struct Chatroom * chatroom[NUM_CLIENT_QUEUE];   // a pointer array of Chatroom (dynamic allocate)
};


/*     Global Variable     */
struct ServerManageInfo Server_MInfo;


/*     Function Declaration     */
int find_room(char * const);
void find_in_room(const char * const, char *);
void strcpyp(char *, char * const, const char * const);
void add_user(const char * const, const char * const);
void add_fd(int, const char * const);
void del_user(int);
void get_userlist(char *);
void insert_userinfo(const char * const, const char * const);
void delete_userinfo(const char * const);

/*---------------------------------MAIN Function---------------------------------*/

int main(int argc, char * argv[]) {

    int server_socket_fd = 0, client_socket_fd = 0, port = 0;
    int i, addrlen = 0;
    char buf[BUF_SIZE];
    struct sockaddr_in server_info;
    struct sockaddr_in client_info;
    fd_set readfds, testfds;
    memset(buf, 0, BUF_SIZE);


    /* arguments preprocess */
    if (argc > 1) {
        port = atoi(argv[1]);
    } else {
        port = DEFAULT_PORT;
    }


    /* init SMI block */
    Server_MInfo.fd_size = 3;
    Server_MInfo.num_user = 0;
    Server_MInfo.user = NULL;
    Server_MInfo.fd_user = (struct Fd_User *) malloc(sizeof(struct Fd_User) * 3);
    for (i = 0; i < NUM_CLIENT_QUEUE; i++) Server_MInfo.chatroom[i] = NULL;


    /* create socket */
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0); // declare socket file description (IPv4, TCP)

    if (server_socket_fd == -1) { // create socket failed
        printf(ERR_SOCKET_CREATE);
        exit(1);
    }


    /* initialize server socket */
    addrlen = sizeof(client_info);
    bzero(&server_info, sizeof(server_info));
    server_info.sin_family = PF_INET; 
    server_info.sin_addr.s_addr = INADDR_ANY; // don't care local address (OS define)
    server_info.sin_port = htons(port);
    bind(server_socket_fd, (struct sockaddr *) &server_info, sizeof(server_info)); // input connection info into socket
    listen(server_socket_fd, NUM_CLIENT_QUEUE); // listening socket
    FD_ZERO(&readfds);
    FD_SET(server_socket_fd, &readfds);
    printf(SUCC_START_LISTEN);


    /* server running */
    while (1) {

        int fd;
        int nread;
        int result;
        testfds = readfds;
        
        result = select(FD_SETSIZE, &testfds, (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0);
        if (result < 1) {
            perror("Error");
            exit(1);
        }

        for (fd = 0; fd < FD_SETSIZE; fd++) {

            if (FD_ISSET(fd, &testfds)) {

                if (fd == server_socket_fd) {

                    addrlen = sizeof(client_info);
                    client_socket_fd = accept(server_socket_fd, (struct sockaddr *) &client_info, (socklen_t *) &addrlen);
                    FD_SET(client_socket_fd, &readfds);
                    printf(SUCC_ADD_CLIENT"%d\n", client_socket_fd);

                }

                else {

                    ioctl(fd, FIONREAD, &nread);

                    if (nread == 0) {
                        
                        close(fd);
                        FD_CLR(fd, &readfds);
                        printf(SUCC_REM_CLIENT"%d\n", fd);

                    }

                    else {

                        char * pch;
                        char tmp_name[NAME_LENGTH];
                        char tmp_room[NAME_LENGTH];
                        char userlist[BUF_SIZE];
                        memset(tmp_name, 0, NAME_LENGTH);
                        memset(tmp_room, 0, NAME_LENGTH);
                        memset(userlist, 0, BUF_SIZE);


                        /* wait for request */
                        recv(fd, buf, sizeof(buf), RECV_NORMAL);


                        /* process request */
                        switch (buf[0]) {

                            case '0': // new user join
                                
                                /* add user info into server */
                                pch = strchr(buf, '\n');
                                strcpyp(tmp_name, &buf[1], pch - 1);
                                strcpy(tmp_room, pch + 1);
                                insert_userinfo(tmp_name, tmp_room); // sign up user info into SMI block (UserInfo)
                                add_user(tmp_name, tmp_room); // sign up user info into SMI block (Chatroom)
                                add_fd(fd, tmp_name); // store fd info (Fd_User)
                                Server_MInfo.num_user++;

                                send(fd, SEND_SUCC_CONNECTION, sizeof(SEND_SUCC_CONNECTION), SEND_NORMAL);
                                printf(SUCC_JOIN, fd, tmp_name, tmp_room);
                                break;

                            case '1': // normal mode
                                break;

                            case '2': // print chatroom member
                                get_userlist(userlist);
                                send(fd, userlist, sizeof(userlist), SEND_NORMAL);
                                break;

                            case '3': // exit
                                Server_MInfo.num_user--;
                                send(fd, SUCC_EXIT, sizeof(SUCC_EXIT), SEND_NORMAL);
                                del_user(fd);
                                break;

                            default:
                                break;

                        }
                    }
                }
            }
        }
    }
}

/*---------------------------------Other Functions---------------------------------*/

/*
    Copy string from top to specific range.
    Input:  destination string, source string, specific char
    Output: void
*/
void strcpyp(char * dst, char * const beg, const char * const end) {
    
    int i = 1;
    char * ptr = beg;
    while (ptr != end) {
        i++;
        ptr++;
    }
    strncpy(dst, beg, i);

}

/*
    Get all online user info.
    Input:  userlist (to store)
    Output: void
*/
void get_userlist(char * userlist) {

    struct UserInfo * ptr = Server_MInfo.user;
    
    /* list is empty */
    if (ptr == NULL) {

        strcat(userlist, "\t");
        strcat(userlist, "NULL\n");

    } else {

        /* looping to create list */
        while (ptr != NULL) {
            strcat(userlist, "\t");
            strcat(userlist, ptr -> name);
            strcat(userlist, "\t[");
            strcat(userlist, ptr -> room);
            strcat(userlist, "]\n");
            ptr = ptr -> next;
        }
    
    }
}

/*  
    Find somebody in which chatroom.
    Input:  username, roomname (to store)
    Output: void
*/
void find_in_room(const char * const name, char * roomname) {

    struct UserInfo * ptr = Server_MInfo.user;

    if (ptr == NULL) { // list is empty.
        memset(roomname, 0, NAME_LENGTH);
    } else {
        while (strcmp(ptr->name, name) != 0) {
            if (ptr -> next == NULL) {
                memset(roomname, 0, NAME_LENGTH);
                return;
            } else {
                ptr = ptr -> next;
            }
        }
        strcpy(roomname, ptr -> room);
    }
}

/* 
    Add new user in Chatroom
    Input:  username, roomname
    Output: void
*/
void add_user(const char * const name, const char * const room) {
    
    int i;

    /* find whether room exist */
    for (i = 0; i < NUM_CLIENT_QUEUE; i++) {
        if (Server_MInfo.chatroom[i] != NULL) {
            if (strcmp(Server_MInfo.chatroom[i] -> roomname, room) == 0) break;
        }
    }

    /* if room not exist */
    if (i == NUM_CLIENT_QUEUE) {
        
        /* create room */
        struct Chatroom * newnode = (struct Chatroom *) malloc(sizeof(struct Chatroom));
        newnode -> num_user = 1;
        strcpy(newnode -> roomname, room);
        for (i = 0; i < NUM_CLIENT_QUEUE; i++) {
            memset(newnode -> user[i], 0, NAME_LENGTH);
        }
        strcpy(newnode -> user[0], name);

        /* find empty index */
        for (i = 0; i < NUM_CLIENT_QUEUE; i++) {

            if (Server_MInfo.chatroom[i] == NULL) {
                Server_MInfo.chatroom[i] = newnode;
            }
        }
    }
    
    /* if room exist */
    else {
        
        int j;

        /* find empty index */
        for (j = 0; j < NUM_CLIENT_QUEUE; j++) {

            if (Server_MInfo.chatroom[i] -> user[j][0] == '\0') {
                strcpy(Server_MInfo.chatroom[i] -> user[j], name); // fill user info into empty index
                Server_MInfo.chatroom[i] -> num_user++;
                break;
            }

        }

        /* list is full */
        // no operation (impossible state at present)
    }
    
}

/*
    Add new client fd into SMI block. (Fd_User)
    Input:  client fd, username
    Output: void
*/
void add_fd(int fd, const char * const name) {
    
    /* index out of current size */
    if (fd > Server_MInfo.fd_size) {

        Server_MInfo.fd_user = (struct Fd_User *) realloc(Server_MInfo.fd_user, sizeof(struct Fd_User) * (fd + 1));
        if (Server_MInfo.fd_user == NULL) {
            printf(ERR_OUT_OF_MEMORY);
            exit(1);
        }
        memset(Server_MInfo.fd_user[fd].name, 0, NAME_LENGTH);
        strcpy(Server_MInfo.fd_user[fd].name, name);

    } 
    
    /* target index exist */
    else {
        strcpy(Server_MInfo.fd_user[fd].name, name);
    }

}

/*
    Delete user in SMI block
    Input:  client fd
    Output: void
*/
void del_user(int fd) {

    int i, j;
    char name[NAME_LENGTH];
    char room[NAME_LENGTH];
    memset(name, 0, NAME_LENGTH);
    memset(room, 0, NAME_LENGTH);
    strcpy(name, Server_MInfo.fd_user[fd].name);
    find_in_room(name, room);

    /* delete in UserInfo */
    delete_userinfo(name); 

    /* delete in Chatroom */
    for (i = 0; i < NUM_CLIENT_QUEUE; i++) {

        if (Server_MInfo.chatroom[i] != NULL) {

            if (strcmp(Server_MInfo.chatroom[i] -> roomname, room) == 0) {
                
                /* delete user in room */
                for (j = 0; i < NUM_CLIENT_QUEUE; i++) {

                    if (strcmp(Server_MInfo.chatroom[i] -> user[j], name) == 0) {

                        memset(Server_MInfo.chatroom[i] -> user[j], 0, NAME_LENGTH);
                        Server_MInfo.chatroom[i] -> num_user--;

                        /* room is empty, delete it */
                        if (Server_MInfo.chatroom[i] -> num_user == 0) {
                            struct Chatroom * ptr = Server_MInfo.chatroom[i];
                            free(ptr);
                            Server_MInfo.chatroom[i] = NULL;
                        }
                        return;
                    }
                }
            }
        }
    }

    /* delete in Fd_User */
    memset(Server_MInfo.fd_user[fd].name, 0, NAME_LENGTH);

}

/*
    Insert a node into UserInfo structure.
    Input:  username, roomname
    Output: void
*/
void insert_userinfo(const char * const name, const char * const room) {
    
    struct UserInfo * ptr = Server_MInfo.user;
    struct UserInfo * newnode;

    newnode = (struct UserInfo *) malloc(sizeof(struct UserInfo));
    memset(newnode -> name, 0, NAME_LENGTH);
    memset(newnode -> room, 0, NAME_LENGTH);
    strcpy(newnode -> name, name);
    strcpy(newnode -> room, room);
    newnode -> prev = NULL;
    newnode -> next = NULL;

    /* list is empty */
    if (Server_MInfo.user == NULL) {     
        Server_MInfo.user = newnode;
    }

    /* list is not empty */
    else {
        while (ptr -> next != NULL) {
            ptr = ptr -> next;
        }
        ptr -> next = newnode;
        ptr -> next -> prev = ptr;
    }
}

/*
    Delete node in UserInfo.
    Input:  username
    Output: void
*/
void delete_userinfo(const char * const name) {
    
    struct UserInfo * ptr = Server_MInfo.user;
    
    while (ptr != NULL) {

        if (strcmp(ptr -> name, name) == 0) {

            /* top node */
            if (ptr -> prev == NULL) {
                Server_MInfo.user = NULL;
            } 
            
            /* tail node */
            else if (ptr -> next == NULL) {
                ptr -> prev -> next = NULL;
            }

            /* midden node */
            else {
                ptr -> prev -> next = ptr -> next;
                ptr -> next -> prev = ptr -> prev;
            }
            
            free(ptr);
            break;

        }
        ptr = ptr -> next;
    }
}