/*     Dependent Library     */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*     Definition Parameters     */
#define SEND_NORMAL 0
#define RECV_NORMAL 0
#define ADDR_SIZE 16
#define BUF_SIZE 1000
#define NAME_LENGTH  1000
#define TIMEOUT_SEC 5
#define TIMEOUT_USEC 0
#define DEFAULT_PORT 12345
#define LOCALHOST "127.0.0.1"
#define MSG_PROMPT "> "
#define MSG_LINE "==============================================================\n"
#define MSG_WELCOME "####    Welcome to KChat    ####\n"
#define MSG_INPUT_NAME "[System]: Please enter your name\n"
#define MSG_INPUT_ROOM "[System]: Please enter your room (if doesn't exist, then create)\n"
#define MSG_PRINT_MEMBER "####    Member List    ####\n"
#define ERR_SOCKET_CREATE "[System]: Fail to create a socket.\n"
#define ERR_CONNECTION "[System]: Connection error.\n"
#define ERRNO_115 "Connection timeout."


/*     Data Structure     */


/*     Global Variable     */
int socket_fd = 0;

/*     Function Declaration     */
void print_member(void);
void del_newline(char *);

/*---------------------------------MAIN Function---------------------------------*/

int main(int argc, char *argv[]) {
    
    int port = 0, err_code = 0, fin = 0;
    char addr[ADDR_SIZE], msg[NAME_LENGTH], buf[BUF_SIZE], name[NAME_LENGTH], room[NAME_LENGTH];
    struct sockaddr_in info;
    struct timeval timeout;

    memset(buf, 0, sizeof(msg));
    memset(msg, 0, sizeof(buf));
    memset(name, 0, sizeof(name));
    memset(addr, 0, sizeof(addr));


    /* arguments preprocess */
    if (argc > 1) {

        /* user defined ip & port num */
        /* addr has provide */
        if (argc > 2) {
            strcpy(addr, argv[1]);
            port = atoi(argv[2]);
        } 
        
        /* only provide port num */
        else {
            strcpy(addr, LOCALHOST);
            port = atoi(argv[1]);
        }
        
    } else {
        
        /* default settings */
        port = DEFAULT_PORT;
        strcpy(addr, LOCALHOST);
    }


    /* create socket */
    socket_fd = socket(AF_INET, SOCK_STREAM, 0); // declare socket file description (IPv4, TCP)

    if (socket_fd == -1) { // create socket failed
        printf(ERR_SOCKET_CREATE);
        exit(1);
    }


    /* initialize socket info */
    bzero(&info, sizeof(info));
    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(addr);
    info.sin_port = htons(port);
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = TIMEOUT_USEC;
    setsockopt (socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)); // set receive timeout
    setsockopt (socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)); // set send timeout


    /* make connection */
    err_code = connect(socket_fd, (struct sockaddr *) &info, sizeof(info)); // connect to server
    if (err_code == -1) {
        if (errno == 115) printf("[System]: "ERRNO_115"\n");
        else printf("[System]: %s\n", strerror(errno));
        exit(1);
    }


    /* input personal info */
    printf("\n"MSG_WELCOME"\n");
    printf(MSG_INPUT_NAME);
    printf(MSG_PROMPT);
    fgets(name, NAME_LENGTH, stdin);
    del_newline(name);
    print_member();
    printf(MSG_INPUT_ROOM);
    printf(MSG_PROMPT);
    fgets(room, NAME_LENGTH, stdin);
    del_newline(room);


    /* prepare request for login */
    msg[0] = '0';
    strcat(msg, name);
    strcat(msg, "\n\0");
    strcat(msg, room); 
    /*** Request Message: 0<username>\n<roomname> (code: 0) ***/


    /* communicate with server */
    do {

        /* "exit" command */
        if (strcmp(msg, "exit") == 0) {
            msg[0] = '3';
            fin = 1;
        }

        /* send & receive message */
        send(socket_fd, msg, sizeof(msg), SEND_NORMAL);
        recv(socket_fd, buf, sizeof(buf), RECV_NORMAL);
        printf("[RECV]Server: %s\n", buf);
        if (fin == 1) break;
        
        printf(MSG_PROMPT);
        memset(buf, 0, sizeof(msg));
        memset(msg, 0, sizeof(buf));

    } while (scanf("%s", msg) != EOF); // while no input then terminate


    /* close connection */
    close(socket_fd);
    exit(0);
}

/*---------------------------------Other Functions---------------------------------*/

/*
    Delete newline character at the bottom.
    Input:  an char array
    Output: void
*/
void del_newline(char * arr) {

    if (arr[strlen(arr) - 1] == '\n') {
        arr[strlen(arr) - 1] = '\0';
    }

}

/*
    Print online member list.
    Input:  void
    Output: void
*/
void print_member(void) {

    char msg[2] = "2\0";
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    send(socket_fd, msg, sizeof(msg), SEND_NORMAL); // send request for member list (code:2)
    recv(socket_fd, buf, sizeof(buf), RECV_NORMAL); // receive from server

    printf("\n"MSG_PRINT_MEMBER"\n");
    printf("%s\n", buf);
    printf(MSG_LINE"\n");
    printf("Usage:\t<Message>")
}
