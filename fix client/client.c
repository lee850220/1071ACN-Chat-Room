/* ################################ USAGE ################################# 
 * ./client             // with default setting (port=12345, ip=localhost)
 * ./client <port>      // port=<port> ip=localhost
 * ./client <ip> <port> // port=<port> ip=<ip>
 * ######################################################################## */

// Known BUG: input words will diminish when get some message from server on typing. (technical obstacle) 

/*     Dependent Library     */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*     Definition Parameters     */
#define SEND_NORMAL         0
#define RECV_NORMAL         0
#define REQ_LOGIN           0
#define REQ_NORMAL          1
#define REQ_SPEC            2
#define REQ_EXIT            3
#define ADDR_SIZE           16
#define BUF_SIZE            1000
#define NAME_LENGTH         1000
#define TIMEOUT_SEC         5
#define TIMEOUT_USEC        0
#define DEFAULT_PORT        12345
#define LOCALHOST           "127.0.0.1"

#define ERASE_LINE          "\33[2K\r"
#define CLEAR               "\33[H\33[2J"
#define COLOR_RESET         "\033[0m"
#define COLOR_B_YELLOW      "\033[1;33m"
#define COLOR_B_LIGHTBLUE   "\033[1;36m"
#define MSG_PROMPT          "\r"COLOR_B_YELLOW"%s > "COLOR_RESET, name
#define MSG_PROMPT_PRE      COLOR_B_YELLOW"> "COLOR_RESET
#define MSG_EXIT            "\n\nBYE!\n\n"
#define MSG_LINE            "==============================================================\n\n"
#define MSG_USAGE           "Usage:\t<Message>\n\t/W <Name or room>;<Message>\n\tBye\n\n"
#define MSG_WELCOME         "\n####    Welcome to KChat    ####\n\n"
#define MSG_INPUT_NAME      COLOR_B_LIGHTBLUE"[System]: Please enter your name\n"COLOR_RESET
#define MSG_INPUT_ROOM      COLOR_B_LIGHTBLUE"[System]: Please enter your room (if doesn't exist, then create)\n"COLOR_RESET
#define MSG_PRINT_MEMBER    "\n####    Member List    ####\n\n"
#define MSG_TYPE0           ERASE_LINE""COLOR_B_LIGHTBLUE"[System]: %s\n"COLOR_RESET, msg
#define MSG_TYPE1           ERASE_LINE"%s > %s\n", tmp_name, msg

#define REQ_PRINT_MEM_MSG   "4\0"
#define REQ_EXIT_MSG        "Bye"

#define ERR_SOCKET_CREATE   "[System]: Fail to create a socket.\n"
#define ERR_CONNECTION      "[System]: Connection error.\n"
#define ERR_DEFAULT_MSG     "[System]: %s\n", strerror(errno)
#define ERRNO_115           "[System]: Connection timeout.\n"


/*     Data Structure     */


/*     Global Variable     */
int socket_fd = 0;
char name[NAME_LENGTH];

/*     Function Declaration     */
void strcpyp(char *, char * const, const char * const);
char itoc(int);
char * find_char(char * const, char);
void print_member(void);
void del_newline(char *);
void recvsocket(void);

/*---------------------------------MAIN Function---------------------------------*/

int main(int argc, char *argv[]) {
    
    int port = 0, err_code = 0, fin = 0, beg = 1;
    char addr[ADDR_SIZE], msg[NAME_LENGTH], buf[BUF_SIZE], room[NAME_LENGTH];
    struct sockaddr_in info;
    struct timeval timeout;
    pthread_t pid;

    memset(buf, 0, sizeof(msg));
    memset(msg, 0, sizeof(buf));
    memset(name, 0, sizeof(name));
    memset(addr, 0, sizeof(addr));
    //printf(CLEAR);


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
    //setsockopt (socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)); // set receive timeout
    setsockopt (socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)); // set send timeout


    /* make connection */
    err_code = connect(socket_fd, (struct sockaddr *) &info, sizeof(info)); // connect to server
    if (err_code == -1) {
        if (errno == 115) printf(ERRNO_115);
        else printf(ERR_DEFAULT_MSG);
        exit(1);
    }


    /* input personal info */
    printf(MSG_WELCOME);
    printf(MSG_INPUT_NAME);
    printf(MSG_PROMPT_PRE);
    fgets(name, NAME_LENGTH, stdin); // get username
    del_newline(name);
    print_member();
    printf(MSG_INPUT_ROOM);
    printf(MSG_PROMPT_PRE);
    fgets(room, NAME_LENGTH, stdin); // get roomname
    del_newline(room);


    /* prepare request for login */
    msg[0] = itoc(REQ_LOGIN);
    strcat(msg, name);
    strcat(msg, "\n\0");
    strcat(msg, room); 
    /*** Request Message: 0<username>\n<roomname> (code: 0) ***/
    pthread_create(&pid, NULL, (void *)recvsocket, NULL);


    /* communicate with server */
    while (1) {

        /* request preprocess */
        if (beg == 1) {                                 // first command no operation (login)
            beg = 0;
        } else if (msg[0] == '\0') {                    // no input
            
            printf(MSG_PROMPT);
            fgets(msg, BUF_SIZE, stdin);                // get new message
            del_newline(msg);
            continue;

        } else if (msg[0] == '/' && msg[1] == 'W' && msg[2] == ' ') { // specific target command

            char * pch;
            char buf2[BUF_SIZE];
            memset(buf2, 0, BUF_SIZE);
            pch = find_char(msg, ';');
            if (pch == NULL) goto NORMAL;
            buf2[0] = itoc(REQ_SPEC); 
            strcpyp(buf, &msg[3], pch - 1);
            strcat(buf2, buf);
            strcat(buf2, "\n");
            strcat(buf2, pch + 1);
            strcpy(msg, buf2);

        } else if (strcmp(msg, REQ_EXIT_MSG) == 0) {    // exit command

            msg[0] = itoc(REQ_EXIT);
            fin = 1;

        } else {                                        // normal mode

            NORMAL:
            strcpy(buf, msg);
            memset(msg, 0, sizeof(msg));
            msg[0] = itoc(REQ_NORMAL);
            strcat(msg, buf);

        }

        /* send & receive message */
        send(socket_fd, msg, sizeof(msg), SEND_NORMAL);
        
        if (fin == 1) break;
        
        printf(MSG_PROMPT);
        memset(buf, 0, sizeof(msg));
        memset(msg, 0, sizeof(buf));
        fgets(msg, BUF_SIZE, stdin);                    // get new message
        del_newline(msg);

    }

    pthread_join(pid, NULL);; // wait for thread to get server return "exit"

    /* close connection */
    close(socket_fd);
    exit(0);
}

/*---------------------------------Thread Functions---------------------------------*/

void recvsocket(void) { // to get message from server

    char * pch;
    char tmp_name[NAME_LENGTH];
    char buf[BUF_SIZE], msg[BUF_SIZE];
    setbuf(stdout, NULL);

    while (1) {

        memset(tmp_name, 0, sizeof(name));
        memset(buf, 0, sizeof(buf));
        memset(msg, 0, sizeof(msg));
        recv(socket_fd, buf, sizeof(buf), RECV_NORMAL);

        switch (buf[0]) {

            case '0': // system message

                strcpy(msg, &buf[1]);
                printf(MSG_TYPE0);
                printf(MSG_PROMPT);
                break;

            case '1': // normal mode (1<username><message>)

                pch = strchr(buf, '\n');
                strcpyp(tmp_name, &buf[1], pch - 1); // take speaker's name
                strcpy(msg, pch + 1);
                printf(MSG_TYPE1); // erase line & print
                printf(MSG_PROMPT);
                break;

            case '3': // get exit return
                printf(MSG_EXIT);
                pthread_exit(0);
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
    Transfer one digit number to char.
    Input:  one digit number
    Output: char
*/
char itoc(int num) {
    return num + '0';
}

/*
    Find specific char in array.
    Input:  char array, spec char
    Output: pointer to spec char
*/
char * find_char(char * const p, char ch) {

    int i;
    for (i = 0; i < strlen(p); i++) {
        if (p[i] == ch) break;
    }
    if (i == strlen(p) && p[i] != ch) return NULL;
    else return p + i;
}

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

    char msg[2] = REQ_PRINT_MEM_MSG;
    char buf[BUF_SIZE];
    memset(buf, 0, BUF_SIZE);

    send(socket_fd, msg, sizeof(msg), SEND_NORMAL); // send request for member list (code:2)
    recv(socket_fd, buf, sizeof(buf), RECV_NORMAL); // receive from server

    printf(MSG_PRINT_MEMBER);
    printf("%s\n", buf);
    printf(MSG_LINE);
    printf(MSG_USAGE);
    printf(MSG_LINE);
}
