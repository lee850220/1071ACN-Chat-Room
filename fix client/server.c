/* ########################### USAGE ########################### 
 * ./server             // with default setting (port=12345)
 * ./server <port>      // port=<port>
 * ############################################################# */

/*     Dependent Library     */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*     Definition Parameters     */
#define DEFAULT_PORT            12345
#define BUF_SIZE                1000
#define NAME_LENGTH             1000
#define SEND_SOCKET_NORMAL      0
#define RECV_SOCKET_NORMAL      0

#define SEND_SYSTEM             0
#define SEND_NORMAL             1

#define SUCC_EXIT               "3BYE!\n"
#define SUCC_JOIN               "[System]: ClientFD="COLOR_B_YELLOW"%d"COLOR_NORMAL": "COLOR_B_YELLOW"%s"COLOR_NORMAL" [%s]\n"
#define SUCC_ADD_CLIENT         "[System]: Adding client on fd "
#define SUCC_REM_CLIENT         "[System]: Removing client on fd "
#define SUCC_START_LISTEN       "[System]: Waiting for client connect...\n"
#define SUCC_RECV_LOGIN         " is join KChat!"

#define ERR_SOCKET_CREATE       "[System]: Fail to create a socket.\n"
#define ERR_CONNECTION          "[System]: Connection error.\n"
#define ERR_TIMEOUT             "[System]: Connection timeout.\n"
#define ERR_OUT_OF_MEMORY       "[System]: Server out of memory.\n"

#define SMSG_SUCC_CONNECTION    "0Login successfully."
#define SMSG_NO_TARGET          "0No such room or user."

#define MSG_RECV                COLOR_B_RED_WHITE"RECV"COLOR_NORMAL
#define MSG_SEND                COLOR_B_LIGHTBLUE"SEND"COLOR_NORMAL
#define MSG_USER                COLOR_B_YELLOW"%s(%d)"COLOR_NORMAL
#define MSG_CODE                COLOR_B_WHITE"Code=%c"COLOR_NORMAL

#define CLEAR                   "\33[H\33[2J"
#define COLOR_NORMAL            COLOR_GREEN_BLACK
#define COLOR_B_WHITE           "\033[1;37m"
#define COLOR_B_RED_WHITE       "\033[1;31;47m"
#define COLOR_B_YELLOW          "\033[1;33m"
#define COLOR_B_LIGHTBLUE       "\033[1;36m"
#define COLOR_GREEN_BLACK       "\033[1;32;40m"

#ifndef NUM_CLIENT_QUEUE
#define NUM_CLIENT_QUEUE        5
#endif

#ifdef __DEBUGGING__

#define SERVERLOG_00 "["MSG_RECV" from fd=%d]: %s\n", fd, buf
#define SERVERLOG_01 "["MSG_RECV" from "MSG_USER"]: %s\n", Server_MInfo.fd_user[fd].name, fd, buf
#define SERVERLOG_02 SERVERLOG_00
#define SERVERLOG_03 SERVERLOG_00
#define SERVERLOG_04 SERVERLOG_00
#define THREADLOG_00 "["MSG_SEND" to   "MSG_USER"]: %s\n", Server_MInfo.fd_user[send_fdset[i]].name, send_fdset[i], msg
#define THREADLOG_01 THREADLOG_00
#define THREADLOG_02 THREADLOG_00
#define THREADLOG_03 THREADLOG_00

#else

#define SERVERLOG_00 "["MSG_RECV" from fd=%d]: "MSG_CODE" %s\n", fd, buf[0], msg
#define SERVERLOG_01 "["MSG_RECV" from "MSG_USER"]: "MSG_CODE" Msg=%s\n", Server_MInfo.fd_user[fd].name, fd, buf[0], msg
#define SERVERLOG_02 "["MSG_RECV" from "MSG_USER"]: "MSG_CODE" target="COLOR_B_YELLOW"%s"COLOR_NORMAL" Msg=%s\n", Server_MInfo.fd_user[fd].name, fd, buf[0], buf2, msg
#define SERVERLOG_03 "["MSG_RECV" from "MSG_USER"]: "MSG_CODE" exit\n", Server_MInfo.fd_user[fd].name, fd, buf[0]
#define SERVERLOG_04 "["MSG_RECV" from fd=%d]: "MSG_CODE" \n", fd, buf[0]
#define THREADLOG_00 "["MSG_SEND" to   "MSG_USER"]: Msg=%s\n", Server_MInfo.fd_user[send_fdset[j]].name, send_fdset[j], &msg[1]
#define THREADLOG_01 "["MSG_SEND" to   "MSG_USER"]: Msg=%s\n", Server_MInfo.fd_user[send_fdset[i]].name, send_fdset[i], buf
#define THREADLOG_02 THREADLOG_01
#define THREADLOG_03 THREADLOG_00

#endif

/*     Data Structure     */
struct Thread_arg {
    int fd;
    char * buf;
};

struct Fd_User {                                // username index by file description
    char name[NAME_LENGTH];
};

struct UserInfo {                               // linked list userinfo block
    int fd;
    char name[NAME_LENGTH];
    char room[NAME_LENGTH];
    struct UserInfo * prev;
    struct UserInfo * next;
};

struct Chatroom {                               // chatroom info block
    int num_user;
    char room[NAME_LENGTH];
    char user[NUM_CLIENT_QUEUE][NAME_LENGTH];   // an array of user name (char array)
};

struct ServerManageInfo {                       // server manage info (SMI) block
    int max_fd;
    int num_user;
    struct UserInfo * user;                         // a pointer of UserInfo (dynamic allocate)
    struct Fd_User * fd_user;                       // a pointer of username (index by fd) (dynamic allocate)
    struct Chatroom * chatroom[NUM_CLIENT_QUEUE];   // a pointer array of Chatroom (dynamic allocate)
};


/*     Global Variable     */
struct ServerManageInfo Server_MInfo;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


/*     Function Declaration     */
char itoc(int);
void process(struct Thread_arg *);
int find_room_index(const char * const);
void find_in_room(const char * const, char *);
void find_chatroom_for_fd(const char * const, int *);
void strcpyp(char *, char * const, const char * const);
void add_user(const char * const, const char * const);
void add_fd(int, const char * const);
void del_user(int);
void get_userlist(char *);
void insert_userinfo(int, const char * const, const char * const);
void delete_userinfo(const char * const);

/*---------------------------------MAIN Function---------------------------------*/

int main(int argc, char * argv[]) {

    int server_socket_fd = 0, client_socket_fd = 0, port = 0;
    int i, count = 0, addrlen = 0;
    char buf[BUF_SIZE], msg[BUF_SIZE];
    struct sockaddr_in server_info;
    struct sockaddr_in client_info;
    struct Thread_arg t_arg;
    fd_set readfds, checkfds;
    pthread_t pid[BUF_SIZE];
    printf(COLOR_NORMAL);
    //printf(CLEAR);

    
    /* arguments preprocess */
    if (argc > 1) {
        port = atoi(argv[1]);
    } else {
        port = DEFAULT_PORT;
    }


    /* init SMI block */
    Server_MInfo.max_fd = 3;
    Server_MInfo.num_user = 0;
    Server_MInfo.user = NULL;
    Server_MInfo.fd_user = (struct Fd_User *) malloc(sizeof(struct Fd_User) * 4);
    for (i = 0; i < NUM_CLIENT_QUEUE; i++) Server_MInfo.chatroom[i] = NULL;


    /* create socket */
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0); // declare socket file description (IPv4, TCP)

    if (server_socket_fd == -1) {               // create socket failed
        printf(ERR_SOCKET_CREATE);
        exit(1);
    }


    /* initialize server socket */
    bzero(&server_info, sizeof(server_info));
    server_info.sin_family = PF_INET;           // same effect as AF_INET
    server_info.sin_addr.s_addr = INADDR_ANY;   // don't care local address (OS define)
    server_info.sin_port = htons(port);
    bind(server_socket_fd, (struct sockaddr *) &server_info, sizeof(server_info)); // input connection info into socket
    listen(server_socket_fd, NUM_CLIENT_QUEUE); // listening socket
    FD_ZERO(&readfds);                          // clear monitor fd set
    FD_SET(server_socket_fd, &readfds);         // put server socket fd into monitor set
    printf(SUCC_START_LISTEN);


    /* server running */
    while (1) {

        int fd;
        int nread;
        int result;
        checkfds = readfds;
        //memset(buf, 0, BUF_SIZE);
        memset(msg, 0, BUF_SIZE);
        
        result = select(FD_SETSIZE, &checkfds, (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0); // monitor fd for read

        /* fd error detect */
        if (result < 1) { 

            if (result == 0) printf(ERR_TIMEOUT);
            else printf("[System]: %s\n", strerror(errno));
            exit(1);

        }


        /* scan all fd */
        for (fd = 0; fd < FD_SETSIZE; fd++) {

            /* fd in checkfds */
            if (FD_ISSET(fd, &checkfds)) {

                /* for server socket */
                if (fd == server_socket_fd) {

                    addrlen = sizeof(client_info);
                    client_socket_fd = accept(server_socket_fd, (struct sockaddr *) &client_info, (socklen_t *) &addrlen); // accept new client
                    FD_SET(client_socket_fd, &readfds); // add client socket fd to monitor set
                    printf(SUCC_ADD_CLIENT"%d\n", client_socket_fd);

                }

                /* for client socket */
                else {

                    ioctl(fd, FIONREAD, &nread);    // check fd read buffer
                    
                    /* buffer empty, client close socket */
                    if (nread == 0) {
                        
                        close(fd);                  // close client socket
                        FD_CLR(fd, &readfds);       // delete fd from monitor set
                        /*** lock thread ***/
                        pthread_mutex_lock(&mutex); 
                        del_user(fd);               // delete user in SMI block
                        pthread_mutex_unlock(&mutex); 
                        /*** unlock thread ***/
                        printf(SUCC_REM_CLIENT"%d\n", fd);

                    }

                    /* process request from client */
                    else {
                        
                        char * pch;
                        char buf2[BUF_SIZE];
                        recv(fd, buf, sizeof(buf), RECV_SOCKET_NORMAL); // wait for request message

                        /* server monitor console */
                        switch (buf[0]) {

                            case '0': // login
                                
                                memset(buf2, 0, BUF_SIZE);
                                pch = strchr(buf, '\n');
                                strcpyp(buf2, &buf[1], pch - 1);
                                strcpy(msg, "name=");
                                strcat(msg, buf2);
                                strcpy(buf2, pch + 1);
                                strcat(msg, " room=");
                                strcat(msg, buf2);
                                printf(SERVERLOG_00);
                                break;
                            
                            case '1': // normal mode

                                strcpy(msg, &buf[1]);
                                printf(SERVERLOG_01);
                                break;
                            
                            case '2': // spec mode
                                memset(buf2, 0, BUF_SIZE);
                                pch = strchr(buf, '\n');
                                strcpyp(buf2, &buf[1], pch - 1);
                                strcpy(msg, pch + 1);
                                printf(SERVERLOG_02);
                                break;
                            
                            case '3': // exit

                                printf(SERVERLOG_03);
                                break;

                            case '4': // print member list

                                printf(SERVERLOG_04);
                                break;

                        }                           
                        
                        t_arg.fd = fd;
                        t_arg.buf = buf;
                        pthread_create(&pid[count], NULL, (void *)process, &t_arg); // start process request
                        pthread_detach(pid[count]);

                    }
                }
            }
        }
    }
}

/*---------------------------------Thread Functions---------------------------------*/

void process(struct Thread_arg * t_arg) {

    int i, j, dup, num_chatroom_user, index;
    int send_fdset[NUM_CLIENT_QUEUE];
    char * pch;
    char tmp_name[NAME_LENGTH];
    char tmp_room[NAME_LENGTH];
    char msg[BUF_SIZE], buf[BUF_SIZE];
    struct UserInfo * ptr;
    memset(tmp_name, 0, NAME_LENGTH);
    memset(tmp_room, 0, NAME_LENGTH);
    memset(buf, 0, BUF_SIZE);
    memset(msg, 0, BUF_SIZE);
    //memset(msg_p, 0, BUF_SIZE);
    for (i = 0; i < NUM_CLIENT_QUEUE; i++) send_fdset[i] = -1;
    setbuf(stdout, NULL);
    //printf("@%d@", t_arg -> buf[0]);

    /* process request */
    switch (t_arg -> buf[0]) {

        case '0': // new user join
        
            pch = strchr(t_arg -> buf, '\n');
            strcpyp(tmp_name, &t_arg -> buf[1], pch - 1);
            strcpy(tmp_room, pch + 1);

            /* prepare join message to ohters */
            msg[0] = itoc(SEND_SYSTEM);
            strcat(msg, "Client [");
            strcat(msg, tmp_name);
            strcat(msg, "] log in room [");
            strcat(msg, tmp_room);
            strcat(msg, "].");

            /*** lock thread ***/
            pthread_mutex_lock(&mutex); 
            insert_userinfo(t_arg -> fd, tmp_name, tmp_room); // sign up user info into SMI block (UserInfo)
            add_user(tmp_name, tmp_room); // sign up user info into SMI block (Chatroom)

            /* collect all online member's fd (for send someone's join message) */
            i = 0;
            ptr = Server_MInfo.user;
            while (ptr != NULL) {

                send_fdset[i++] = ptr -> fd;
                ptr = ptr -> next;
                
            }

            add_fd(t_arg -> fd, tmp_name); // store fd info (Fd_User)
            Server_MInfo.num_user++;
            pthread_mutex_unlock(&mutex);
            /*** unlock thread ***/

            send(t_arg -> fd, SMSG_SUCC_CONNECTION, sizeof(SMSG_SUCC_CONNECTION), SEND_SOCKET_NORMAL);
            printf(SUCC_JOIN, t_arg -> fd, tmp_name, tmp_room);

            /* send join message to others */
            for (j = 0; j < i; j++) {
                if (send_fdset[j] == t_arg -> fd) continue;
                printf(THREADLOG_00);
                send(send_fdset[j], msg, sizeof(msg), SEND_SOCKET_NORMAL); // transfer msg to all chatroom user
            }
            break;

        case '1': // normal mode

            /* find room */
            /*** lock thread ***/
            pthread_mutex_lock(&mutex);
            strcpy(tmp_name, Server_MInfo.fd_user[t_arg -> fd].name); // get speaker's name
            find_in_room(tmp_name, tmp_room); // use name to find in which room
            strcpyp(buf, &t_arg -> buf[1], &t_arg -> buf[strlen(t_arg -> buf) - 1]);
            msg[0] = itoc(SEND_NORMAL);
            strcat(msg, tmp_name);
            strcat(msg, "\n");
            strcat(msg, buf);

            find_chatroom_for_fd(tmp_room, send_fdset); // find all chatroom socket fd
            pthread_mutex_unlock(&mutex); 
            /*** unlock thread ***/

            num_chatroom_user = Server_MInfo.chatroom[find_room_index(tmp_room)] -> num_user;
            for (i = 0; i < num_chatroom_user; i++) {
                if (send_fdset[i] == t_arg -> fd) continue;
                printf(THREADLOG_01);
                send(send_fdset[i], msg, sizeof(msg), SEND_SOCKET_NORMAL); // transfer msg to all chatroom user
            }
            break;

        case '2': // specific communication
            
            pch = strchr(t_arg -> buf, '\n');
            strcpyp(tmp_name, &t_arg -> buf[1], pch - 1);
            msg[0] = itoc(SEND_NORMAL);
            /*** lock thread ***/
            pthread_mutex_lock(&mutex);
            strcat(msg, Server_MInfo.fd_user[t_arg -> fd].name); // get speaker's name
            strcat(msg, "\n");
            strcat(msg, pch + 1);
            strcpy(buf, pch + 1);

            /* find target is room or name or both */
            index = find_room_index(tmp_name);
            if (index != -1) find_chatroom_for_fd(tmp_name, send_fdset); // put all chatroom member's fd into set

            for (i = 0; i < NUM_CLIENT_QUEUE; i++) { // get last index of nonempty(-1) fd

                if (send_fdset[i] == -1) {
                    index = i;
                    break;
                }

            }

            ptr = Server_MInfo.user;
            while (ptr != NULL) { // put all member's fd which fit the target into set

                if (strcmp(ptr -> name, tmp_name) == 0) {

                    for (j = 0; j < NUM_CLIENT_QUEUE; j++) { // check fd duplicate

                        if (send_fdset[j] == ptr -> fd) {
                            dup = 1;
                            break;
                        }

                    }

                    if (dup == 0) send_fdset[index++] = ptr -> fd;
                    dup = 0;
                }
                ptr = ptr -> next;
            }
            pthread_mutex_unlock(&mutex); 
            /*** unlock thread ***/

            /* send message */
            if (send_fdset[0] == -1) { // no target

                send(t_arg -> fd, SMSG_NO_TARGET, sizeof(SMSG_NO_TARGET), SEND_SOCKET_NORMAL);

            } else {

                for (i = 0; i < index; i++) {
                    if (send_fdset[i] == t_arg -> fd) continue;
                    printf(THREADLOG_02);
                    send(send_fdset[i], msg, sizeof(msg), SEND_SOCKET_NORMAL); // transfer msg to all chatroom user
                }

            }
            break;

        case '3': // exit

            /*** lock thread ***/
            pthread_mutex_lock(&mutex);
            msg[0] = itoc(SEND_SYSTEM);
            strcat(msg, "Client [");
            strcat(msg, Server_MInfo.fd_user[t_arg -> fd].name); // get speaker's name
            strcat(msg, "] leave.");

            /* collect all online member's fd (for send someone's exit message) */
            i = 0;
            ptr = Server_MInfo.user;
            while (ptr != NULL) {

                send_fdset[i++] = ptr -> fd;
                ptr = ptr -> next;
                
            }

            Server_MInfo.num_user--;
            pthread_mutex_unlock(&mutex); 
            /*** unlock thread ***/
            send(t_arg -> fd, SUCC_EXIT, sizeof(SUCC_EXIT), SEND_SOCKET_NORMAL); // send exit message to client which just left

            /* send exit message to others */
            for (j = 0; j < i; j++) {
                if (send_fdset[j] == t_arg -> fd) continue;
                printf(THREADLOG_03);
                send(send_fdset[j], msg, sizeof(msg), SEND_SOCKET_NORMAL); // transfer msg to all chatroom user
            }
            break;
        
        case '4': // print chatroom member

            /*** lock thread ***/
            pthread_mutex_lock(&mutex);
            get_userlist(msg);
            pthread_mutex_unlock(&mutex); 
            /*** unlock thread ***/
            send(t_arg -> fd, msg, sizeof(msg), SEND_SOCKET_NORMAL);
            break;

        default:
            break;

    }

    pthread_exit(0);
}

/*---------------------------------Other Functions---------------------------------*/

/*
    Transfer one digit number to char.
    Input:  one digit number
    Output: char
*/
char itoc(int num) {
    return num + '0';
}

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
    Find chatroom index in SMI block.
    Input:  roomname
    Output: room index
*/
int find_room_index(const char * const room) {

    int i;
    for (i = 0; i < NUM_CLIENT_QUEUE; i++) {
        if (Server_MInfo.chatroom[i] != NULL) 
            if (strcmp(Server_MInfo.chatroom[i]->room, room) == 0)
                return i;
    }
    return -1;

}

/*
    Find all member's client fd in a chatroom.
    Input:  username, client fd array (to store)
    Output: void
*/
void find_chatroom_for_fd(const char * const room, int * fdset) {
    
    int i = 0;
    struct UserInfo * ptr = Server_MInfo.user;

    while (ptr != NULL) {
        
        if (strcmp(ptr -> room, room) == 0) {
            fdset[i++] = ptr -> fd;
        }
        ptr = ptr -> next;

    }

}

/*  
    Find somebody in which chatroom.
    Input:  username, roomname (to store)
    Output: void
*/
void find_in_room(const char * const name, char * room) {

    struct UserInfo * ptr = Server_MInfo.user;

    if (ptr == NULL) { // list is empty.
        memset(room, 0, NAME_LENGTH);
    } else {
        while (strcmp(ptr->name, name) != 0) {
            if (ptr -> next == NULL) {
                memset(room, 0, NAME_LENGTH);
                return;
            } else {
                ptr = ptr -> next;
            }
        }
        strcpy(room, ptr -> room);
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
            if (strcmp(Server_MInfo.chatroom[i] -> room, room) == 0) break;
        }
    }

    /* if room not exist */
    if (i == NUM_CLIENT_QUEUE) {
        
        /* create room */
        struct Chatroom * newnode = (struct Chatroom *) malloc(sizeof(struct Chatroom));
        newnode -> num_user = 1;
        strcpy(newnode -> room, room);
        for (i = 0; i < NUM_CLIENT_QUEUE; i++) {
            memset(newnode -> user[i], 0, NAME_LENGTH);
        }
        strcpy(newnode -> user[0], name);

        /* find empty index */
        for (i = 0; i < NUM_CLIENT_QUEUE; i++) {

            if (Server_MInfo.chatroom[i] == NULL) {
                Server_MInfo.chatroom[i] = newnode;
                break;
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
    if (fd > Server_MInfo.max_fd) {

        Server_MInfo.fd_user = (struct Fd_User *) realloc(Server_MInfo.fd_user, sizeof(struct Fd_User) * (fd + 1));
        Server_MInfo.max_fd = fd;
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

            if (strcmp(Server_MInfo.chatroom[i] -> room, room) == 0) {
                
                /* delete user in room */
                for (j = 0; j < NUM_CLIENT_QUEUE; j++) {

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
void insert_userinfo(int fd, const char * const name, const char * const room) {
    
    struct UserInfo * ptr = Server_MInfo.user;
    struct UserInfo * newnode;

    newnode = (struct UserInfo *) malloc(sizeof(struct UserInfo));
    memset(newnode -> name, 0, NAME_LENGTH);
    memset(newnode -> room, 0, NAME_LENGTH);
    strcpy(newnode -> name, name);
    strcpy(newnode -> room, room);
    newnode -> fd = fd;
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