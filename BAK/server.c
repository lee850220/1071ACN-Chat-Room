/* ########################### USAGE ########################### 
* ./server             // with default setting (port=12345)
* ./server <port>      // port=<port>
* ############################################################# */

// Known BUG: FD_SETSIZE = 1024 (fixed in linux) (may be solve in future) 

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
#define NAME_LENGTH             100
#define SEND_SOCKET_NORMAL      0
#define RECV_SOCKET_NORMAL      0
#define INIT_CLIENT_NUM         5
#define INIT_CHATROOM_USER      5
#define LISTEN_QUEUE            10
#define NULL_FD                 -1

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

#ifdef __DEBUGGING__

#define SERVERLOG_00 "["MSG_RECV" from fd=%d]: %s\n", fd, buf
#define SERVERLOG_01 "["MSG_RECV" from "MSG_USER"]: %s\n", SMI.fd_user[fd].name, fd, buf
#define SERVERLOG_02 SERVERLOG_00
#define SERVERLOG_03 SERVERLOG_00
#define SERVERLOG_04 SERVERLOG_00
#define THREADLOG_00 "["MSG_SEND" to   "MSG_USER"]: %s\n", SMI.fd_user[send_fdset[i]].name, send_fdset[i], msg
#define THREADLOG_01 THREADLOG_00
#define THREADLOG_02 THREADLOG_00
#define THREADLOG_03 THREADLOG_00

#else

#define SERVERLOG_00 "["MSG_RECV" from fd=%d]: "MSG_CODE" %s\n", fd, buf[0], msg
#define SERVERLOG_01 "["MSG_RECV" from "MSG_USER"]: "MSG_CODE" Msg=%s\n", SMI.fd_user[fd].name, fd, buf[0], msg
#define SERVERLOG_02 "["MSG_RECV" from "MSG_USER"]: "MSG_CODE" target="COLOR_B_YELLOW"%s"COLOR_NORMAL" Msg=%s\n", SMI.fd_user[fd].name, fd, buf[0], buf2, msg
#define SERVERLOG_03 "["MSG_RECV" from "MSG_USER"]: "MSG_CODE" exit\n", SMI.fd_user[fd].name, fd, buf[0]
#define SERVERLOG_04 "["MSG_RECV" from fd=%d]: "MSG_CODE" \n", fd, buf[0]
#define THREADLOG_00 "["MSG_SEND" to   "MSG_USER"]: Msg=%s\n", SMI.fd_user[send_fdset[j]].name, send_fdset[j], &msg[1]
#define THREADLOG_01 "["MSG_SEND" to   "MSG_USER"]: Msg=%s\n", SMI.fd_user[send_fdset[i]].name, send_fdset[i], buf
#define THREADLOG_02 THREADLOG_01
#define THREADLOG_03 THREADLOG_00

#endif

/*     Data Structure     */
typedef char NAME[NAME_LENGTH];

typedef struct {                // thread argumet structure
    int    fd;
    char*  buf;
} Thread_Arg;

typedef struct {                // username index by file description
    NAME   name;
} Fd_User;

typedef struct UserInfo {       // linked list userinfo block
    int    fd;
    NAME   name;
    NAME   room;
    struct UserInfo* prev;
    struct UserInfo* next;
} UserInfo;

typedef struct {                // chatroom info block
    int    num_user;
    NAME   room;
    NAME*  user;   // an array of user name (char array)
} Chatroom;

typedef struct {                // server manage info (SMI) block
    int    max_fd;
    int    num_user;
    int    num_chatroom;
    int    num_arrsize;         // dynamic array size (max clients)
    Fd_User*  fd_user;          // a pointer of username (index by fd) (dynamic allocate)
    UserInfo* userinfo;         // a pointer of UserInfo (dynamic allocate)
    Chatroom** chatroom;        // a pointer array of Chatroom (dynamic allocate)
} ServerManageInfo;


/*     Global Variable     */
ServerManageInfo SMI;
pthread_mutex_t  mutex = PTHREAD_MUTEX_INITIALIZER;


/*     Function Declaration     */
void process(Thread_Arg*);
char itoc(int);
void strcpyp(char*, char* const, const char* const);
int  find_room_index(const char* const);
void find_in_room(const char* const, char*);
void find_chatroom_for_fd(const char* const, int*);
void get_userlist(char*);
void add_fd(int, const char* const);
void add_chatroom(const char* const, const char* const);
void del_user(int);
void insert_userinfo(int, const char* const, const char* const);
void delete_userinfo(const char* const);

/*---------------------------------MAIN Function---------------------------------*/

int main(int argc, char* argv[]) {

    int                 i, addrlen = 0;
    int                 port = 0;
    int                 server_socket_fd = 0;
    int                 client_socket_fd = 0;
    char                buf[BUF_SIZE];
    char                msg[BUF_SIZE];
    struct sockaddr_in  server_info;
    struct sockaddr_in  client_info;
    Thread_Arg          arg;
    fd_set              readfds, checkfds;
    pthread_t           pid;
    printf(COLOR_NORMAL);
    printf(CLEAR);

    
    /* arguments preprocess */
    if (argc > 1) {
        port = atoi(argv[1]); // provide port num
    } else {
        port = DEFAULT_PORT;  // use default setting
    }


    /* init SMI block */
    SMI.max_fd = FD_SETSIZE;
    SMI.num_user = 0;
    SMI.num_chatroom = 0;
    SMI.num_arrsize = INIT_CLIENT_NUM;
    SMI.userinfo = NULL;
    SMI.chatroom = (Chatroom**) malloc(sizeof(Chatroom*)* INIT_CLIENT_NUM);
    SMI.fd_user  = (Fd_User *) malloc(sizeof(Fd_User)* (SMI.max_fd + 1));
    for (i = 0; i < INIT_CLIENT_NUM; i++) SMI.chatroom[i] = NULL;


    /* create socket */
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0); // declare socket file description (IPv4, TCP)
    if (server_socket_fd == -1) {                       // create socket failed
        printf(ERR_SOCKET_CREATE);
        exit(1);
    }


    /* initialize server socket */
    bzero(&server_info, sizeof(server_info));           // init server address block
    server_info.sin_family = PF_INET;                   // same effect as AF_INET
    server_info.sin_addr.s_addr = INADDR_ANY;           // don't care local address (OS define)
    server_info.sin_port = htons(port);
    bind(server_socket_fd, (struct sockaddr *) &server_info, sizeof(server_info)); // put connection info into socket
    listen(server_socket_fd, LISTEN_QUEUE);             // listening socket (for queue)
    FD_ZERO(&readfds);                                  // clear monitor fd set
    FD_SET(server_socket_fd, &readfds);                 // put server socket fd into monitor set
    printf(SUCC_START_LISTEN);


    /* server running */
    while (1) {

        int fd;
        int nread;
        int result;
        checkfds = readfds;
        memset(msg, 0, BUF_SIZE);
        //memset(buf, 0, BUF_SIZE);
        
        result = select(FD_SETSIZE, &checkfds, (fd_set *) 0, (fd_set *) 0, (struct timeval *) 0); // monitor fd change for read

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
                        
                        char* pch;
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
                        
                        arg.fd = fd;
                        arg.buf = buf;
                        pthread_create(&pid, NULL, (void *)process, &arg); // start process request
                        pthread_detach(pid);

                    }
                }
            }
        }
    }
}

/*---------------------------------Thread Functions---------------------------------*/

void process(Thread_Arg* arg) {

    int         i, j, dup, index;
    int         num_chatroom_user;
    NAME        tmp_name;
    NAME        tmp_room;
    char        msg[BUF_SIZE];
    char        buf[BUF_SIZE];
    char*       pch         = NULL;
    UserInfo*   ptr         = NULL;

    memset(tmp_name, 0, NAME_LENGTH);
    memset(tmp_room, 0, NAME_LENGTH);
    memset(buf,      0, BUF_SIZE);
    memset(msg,      0, BUF_SIZE);

    pthread_mutex_lock(&mutex);
    int send_fdset[SMI.num_user];
    for (i = 0; i < SMI.num_user; i++) send_fdset[i] = -1;
    pthread_mutex_unlock(&mutex);
    
    setbuf(stdout, NULL); // close buffering

    /* process request */
    switch (arg->buf[0]) {

        case '0': // new user join
            
            pch = strchr(arg->buf, '\n');
            strcpyp(tmp_name, &arg->buf[1], pch - 1);       // get username
            strcpy(tmp_room, pch + 1);                      // get roomname

            /* prepare join message to ohters */
            msg[0] = itoc(SEND_SYSTEM);
            strcat(msg, "Client [");
            strcat(msg, tmp_name);
            strcat(msg, "] log in room [");
            strcat(msg, tmp_room);
            strcat(msg, "].");

            /*** lock thread ***/
            pthread_mutex_lock(&mutex);

            /* check SMI block size */
            SMI.num_user++;
            if (SMI.num_user >= SMI.num_arrsize) {          // if array is full, extend 5 space
                SMI.num_arrsize += INIT_CLIENT_NUM;
                SMI.chatroom = (Chatroom**) realloc(SMI.userinfo, sizeof(Chatroom*)* (SMI.num_arrsize));
                for (i = SMI.num_arrsize - INIT_CLIENT_NUM; i < SMI.num_arrsize; i++) SMI.chatroom[i] = NULL;
            }

            insert_userinfo(arg->fd, tmp_name, tmp_room);   // sign up user info into SMI block (UserInfo)
            add_chatroom(tmp_name, tmp_room);                   // sign up user info into SMI block (Chatroom)

            /* collect all online member's fd (for send someone's join message) */
            i = 0;
            ptr = SMI.userinfo;
            while (ptr != NULL) {
                send_fdset[i++] = ptr->fd;
                ptr = ptr->next;             
            }

            add_fd(arg->fd, tmp_name);                      // store fd info (Fd_User)
            pthread_mutex_unlock(&mutex);
            /*** unlock thread ***/

            send(arg->fd, SMSG_SUCC_CONNECTION, sizeof(SMSG_SUCC_CONNECTION), SEND_SOCKET_NORMAL); // send login message to client
            printf(SUCC_JOIN, arg->fd, tmp_name, tmp_room);

            /* send join message to others */
            for (j = 0; j < i; j++) {
                if (send_fdset[j] == arg->fd) continue; // don't send to self
                printf(THREADLOG_00);
                send(send_fdset[j], msg, sizeof(msg), SEND_SOCKET_NORMAL); // send msg to all online user
            }
            break;

        case '1': // normal mode

            /* find room */
            /*** lock thread ***/
            pthread_mutex_lock(&mutex);
            strcpy(tmp_name, SMI.fd_user[arg->fd].name); // get speaker's name
            find_in_room(tmp_name, tmp_room); // use name to find in which room
            strcpyp(buf, &arg->buf[1], &arg->buf[strlen(arg->buf) - 1]);
            msg[0] = itoc(SEND_NORMAL);
            strcat(msg, tmp_name);
            strcat(msg, "\n");
            strcat(msg, buf);

            find_chatroom_for_fd(tmp_room, send_fdset); // find all chatroom socket fd
            pthread_mutex_unlock(&mutex); 
            /*** unlock thread ***/

            num_chatroom_user = SMI.chatroom[find_room_index(tmp_room)]->num_user;
            for (i = 0; i < num_chatroom_user; i++) {
                if (send_fdset[i] == arg->fd) continue;
                printf(THREADLOG_01);
                send(send_fdset[i], msg, sizeof(msg), SEND_SOCKET_NORMAL); // transfer msg to all chatroom user
            }
            break;

        case '2': // specific communication
            
            pch = strchr(arg->buf, '\n');
            strcpyp(tmp_name, &arg->buf[1], pch - 1);
            msg[0] = itoc(SEND_NORMAL);
            /*** lock thread ***/
            pthread_mutex_lock(&mutex);
            strcat(msg, SMI.fd_user[arg->fd].name); // get speaker's name
            strcat(msg, "\n");
            strcat(msg, pch + 1);
            strcpy(buf, pch + 1);

            /* find target is room or name or both */
            index = find_room_index(tmp_name);
            if (index != -1) find_chatroom_for_fd(tmp_name, send_fdset); // put all chatroom member's fd into set

            for (i = 0; i < sizeof(send_fdset) / sizeof(send_fdset[0]); i++) { // get last index of nonempty(-1) fd

                if (send_fdset[i] == -1) {
                    index = i;
                    break;
                }

            }

            ptr = SMI.userinfo;
            while (ptr != NULL) { // put all member's fd which fit the target into set

                if (strcmp(ptr->name, tmp_name) == 0) {

                    for (j = 0; j < SMI.num_arrsize; j++) { // check fd duplicate

                        if (send_fdset[j] == ptr->fd) {
                            dup = 1;
                            break;
                        }

                    }

                    if (dup == 0) send_fdset[index++] = ptr->fd;
                    dup = 0;
                }
                ptr = ptr->next;
            }
            pthread_mutex_unlock(&mutex); 
            /*** unlock thread ***/

            /* send message */
            if (send_fdset[0] == -1) { // no target

                send(arg->fd, SMSG_NO_TARGET, sizeof(SMSG_NO_TARGET), SEND_SOCKET_NORMAL);

            } else {

                for (i = 0; i < index; i++) {
                    if (send_fdset[i] == arg->fd) continue;
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
            strcat(msg, SMI.fd_user[arg->fd].name); // get speaker's name
            strcat(msg, "] leave.");

            /* collect all online member's fd (for send someone's exit message) */
            i = 0;
            ptr = SMI.userinfo;
            while (ptr != NULL) {

                send_fdset[i++] = ptr->fd;
                ptr = ptr->next;
                
            }

            SMI.num_user--;
            pthread_mutex_unlock(&mutex); 
            /*** unlock thread ***/
            send(arg->fd, SUCC_EXIT, sizeof(SUCC_EXIT), SEND_SOCKET_NORMAL); // send exit message to client which just left

            /* send exit message to others */
            for (j = 0; j < i; j++) {
                if (send_fdset[j] == arg->fd) continue;
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
            send(arg->fd, msg, sizeof(msg), SEND_SOCKET_NORMAL);
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
void strcpyp(char* dst, char* const beg, const char* const end) {
    
    int i = 1;
    char* ptr = beg;
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
void get_userlist(char* userlist) {

    UserInfo* ptr = SMI.userinfo;
    
    /* list is empty */
    if (ptr == NULL) {

        strcat(userlist, "\t");
        strcat(userlist, "NULL\n");

    } else {

        /* looping to create list */
        while (ptr != NULL) {
            strcat(userlist, "\t");
            strcat(userlist, ptr->name);
            strcat(userlist, "\t[");
            strcat(userlist, ptr->room);
            strcat(userlist, "]\n");
            ptr = ptr->next;
        }
    
    }
}

/*
    Find chatroom index in SMI block.
    Input:  roomname
    Output: room index
*/
int find_room_index(const char* const room) {

    int i;
    for (i = 0; i < SMI.num_arrsize; i++) {
        if (SMI.chatroom[i] != NULL) 
            if (strcmp(SMI.chatroom[i]->room, room) == 0)
                return i;
    }
    return -1;

}

/*
    Find all member's client fd in a chatroom.
    Input:  username, client fd array (to store)
    Output: void
*/
void find_chatroom_for_fd(const char* const room, int* fdset) {
    
    int i = 0;
    UserInfo* ptr = SMI.userinfo;

    while (ptr != NULL) {
        
        if (strcmp(ptr->room, room) == 0) {
            fdset[i++] = ptr->fd;
        }
        ptr = ptr->next;

    }

}

/*  
    Find somebody in which chatroom.
    Input:  username, roomname (to store)
    Output: void
*/
void find_in_room(const char* const name, char* room) {

    UserInfo* ptr = SMI.userinfo;

    if (ptr == NULL) { // list is empty.
        memset(room, 0, NAME_LENGTH);
    } else {
        while (strcmp(ptr->name, name) != 0) {
            if (ptr->next == NULL) {
                memset(room, 0, NAME_LENGTH);
                return;
            } else {
                ptr = ptr->next;
            }
        }
        strcpy(room, ptr->room);
    }
}

/* 
    Add new user in Chatroom
    Input:  username, roomname
    Output: void
*/
void add_chatroom(const char* const name, const char* const room) {
    
    int i;

    /* find whether room exist */
    for (i = 0; i < SMI.num_arrsize; i++) {
        if (SMI.chatroom[i] != NULL) {
            if (strcmp(SMI.chatroom[i]->room, room) == 0) break;
        }
    }

    /* if room not exist */
    if (i == SMI.num_arrsize) {
        
        /* create room */
        Chatroom* newnode = (Chatroom *) malloc(sizeof(Chatroom));
        newnode->num_user = 1;
        strcpy(newnode->room, room);
        newnode->user = (NAME*) malloc(sizeof(NAME) * INIT_CHATROOM_USER);
        for (i = 0; i < SMI.num_arrsize; i++) memset(newnode->user[i], 0, NAME_LENGTH);
        strcpy(newnode->user[0], name);

        /* find empty index */
        for (i = 0; i < SMI.num_arrsize; i++) {

            if (SMI.chatroom[i] == NULL) {
                SMI.chatroom[i] = newnode;
                break;
            }
        }
    }
    
    /* if room exist */
    else {
        
        int j;
        SMI.chatroom[i]->num_user++;

        /* find empty index */
        for (j = 0; j < SMI.num_arrsize; j++) {

            if (SMI.chatroom[i]->user[j][0] == '\0') {
                strcpy(SMI.chatroom[i]->user[j], name); // fill user info into empty index
                return;
            }

        }

        /* list is full, extend 5 space */
        SMI.chatroom[i]->num_user += INIT_CHATROOM_USER;
        SMI.chatroom[i]->user = (NAME*) realloc(SMI.chatroom[i]->user, sizeof(NAME) * SMI.chatroom[i]->num_user);
        for (j = SMI.chatroom[i]->num_user - INIT_CHATROOM_USER; j < SMI.chatroom[i]->num_user; j++) memset(SMI.chatroom[i]->user[j], 0, NAME_LENGTH);
        strcpy(SMI.chatroom[i]->user[SMI.num_user - INIT_CHATROOM_USER], name);

    }
    
}

/*
    Add new client fd into SMI block. (Fd_User)
    Input:  client fd, username
    Output: void
*/
void add_fd(int fd, const char* const name) {
    
    /* index out of current size */
    if (fd > SMI.max_fd) {

        SMI.fd_user = (Fd_User *) realloc(SMI.fd_user, sizeof(Fd_User)* (fd + 1));
        SMI.max_fd = fd;
        if (SMI.fd_user == NULL) {
            printf(ERR_OUT_OF_MEMORY);
            exit(1);
        }
        memset(SMI.fd_user[fd].name, 0, NAME_LENGTH);
        strcpy(SMI.fd_user[fd].name, name);

    } 
    
    /* target index exist */
    else {
        strcpy(SMI.fd_user[fd].name, name);
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
    strcpy(name, SMI.fd_user[fd].name);
    find_in_room(name, room);

    /* delete in UserInfo */
    delete_userinfo(name); 

    /* delete in Chatroom */
    for (i = 0; i < SMI.num_arrsize; i++) {

        if (SMI.chatroom[i] != NULL) {

            if (strcmp(SMI.chatroom[i]->room, room) == 0) {
                
                /* delete user in room */
                for (j = 0; j < SMI.num_arrsize; j++) {

                    if (strcmp(SMI.chatroom[i]->user[j], name) == 0) {

                        memset(SMI.chatroom[i]->user[j], 0, NAME_LENGTH);
                        SMI.chatroom[i]->num_user--;

                        /* room is empty, delete it */
                        if (SMI.chatroom[i]->num_user == 0) {
                            Chatroom* ptr = SMI.chatroom[i];
                            free(ptr);
                            SMI.chatroom[i] = NULL;
                        }
                        return;
                    }
                }
            }
        }
    }

    /* delete in Fd_User */
    memset(SMI.fd_user[fd].name, 0, NAME_LENGTH);

}

/*
    Insert a node into UserInfo structure.
    Input:  username, roomname
    Output: void
*/
void insert_userinfo(int fd, const char* const name, const char* const room) {
    
    UserInfo* ptr = SMI.userinfo;
    UserInfo* newnode;

    newnode = (UserInfo *) malloc(sizeof(UserInfo));
    memset(newnode->name, 0, NAME_LENGTH);
    memset(newnode->room, 0, NAME_LENGTH);
    strcpy(newnode->name, name);
    strcpy(newnode->room, room);
    newnode->fd = fd;
    newnode->prev = NULL;
    newnode->next = NULL;

    /* list is empty */
    if (SMI.userinfo == NULL) {     
        SMI.userinfo = newnode;
    }

    /* list is not empty */
    else {
        while (ptr->next != NULL) {
            ptr = ptr->next;
        }
        ptr->next = newnode;
        ptr->next->prev = ptr;
    }
}

/*
    Delete node in UserInfo.
    Input:  username
    Output: void
*/
void delete_userinfo(const char* const name) {
    
    UserInfo* ptr = SMI.userinfo;
    
    while (ptr != NULL) {

        if (strcmp(ptr->name, name) == 0) {

            /* top node */
            if (ptr->prev == NULL) {
                SMI.userinfo = NULL;
            } 
            
            /* tail node */
            else if (ptr->next == NULL) {
                ptr->prev->next = NULL;
            }

            /* midden node */
            else {
                ptr->prev->next = ptr->next;
                ptr->next->prev = ptr->prev;
            }
            
            free(ptr);
            break;

        }
        ptr = ptr->next;
    }
}