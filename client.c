/* ################################ USAGE ################################# 
 * ./client                 // with default setting (port=12345, ip=localhost)
 * ./client <ip>            // port=12345 ip=<ip>
 * ./client <port>          // port=<port> ip=localhost
 * ./client <ip> <port>     // port=<port> ip=<ip>
 * ######################################################################## */

/*     Dependent Library     */
#include <errno.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <termios.h>
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
#define HISTORY_SIZE        500
#define NAME_LENGTH         1000
#define TIMEOUT_SEC         5
#define TIMEOUT_USEC        0
#define DEFAULT_PORT        12345
#define LOCALHOST           "127.0.0.1"

#define ERASE_LINE          "\33[2K\r"
#define CLEAR               "\33[H\33[2J"
#define CUR_BOTTOM          "\33[100B"
#define UP                  "\33[A"
#define DOWN                "\33[B"
#define LEFT                "\33[D"
#define RIGHT               "\33[C"
#define STORE_CUR           "\33[s"
#define RESTORE_CUR         "\33[u"
#define COLOR_RESET         "\033[0m"
#define COLOR_B_YELLOW      "\033[1;33m"
#define COLOR_B_LIGHTBLUE   "\033[1;36m"
#define MSG_PROMPT          "\r"COLOR_B_YELLOW"%s > "COLOR_RESET, Name
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
int Isrom = 1;

/*     Global Variable     */
int socket_fd = 0, Num_History = 0;
char Name[NAME_LENGTH], Tmpbuf[BUF_SIZE], History[HISTORY_SIZE][BUF_SIZE];

/*     Function Declaration     */
void strcpyp(char *, char * const, const char * const);
char itoc(int);
char * find_char(char * const, char);
void print_member(void);
void del_newline(char *);
void recvsocket(void);
char getch(void);
void getmsg(void);

/*---------------------------------MAIN Function---------------------------------*/

int main(int argc, char *argv[]) {
    
    int i, port = 0, err_code = 0, fin = 0, beg = 1;
    char addr[ADDR_SIZE], msg[NAME_LENGTH], buf[BUF_SIZE], room[NAME_LENGTH];
    struct sockaddr_in info;
    struct timeval timeout;
    pthread_t pid;


    memset(buf, 0, sizeof(msg));
    memset(msg, 0, sizeof(buf));
    memset(Name, 0, sizeof(Name));
    memset(addr, 0, sizeof(addr));
    for (i = 0; i < HISTORY_SIZE; i++) memset(History[i], 0, BUF_SIZE);
    printf(CLEAR);


    /* arguments preprocess */
    if (argc > 1) {

        /* user defined ip & port num */
        /* addr has provide */
        if (argc > 2) {
            strcpy(addr, argv[1]);
            port = atoi(argv[2]);
        } 
        
        /* only provide port or ip */
        else {

            int isIP = 0;
            for (i = 0; i < sizeof(argv[1]) / sizeof(char); i++) {
                if (argv[1][i] == '.') {
                    isIP = 1;
                    break;
                }
            }

            if (isIP) {
                strcpy(addr, argv[1]);
                port = DEFAULT_PORT;
            } else {
                strcpy(addr, LOCALHOST);
                port = atoi(argv[1]);
            }
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
    fgets(Name, NAME_LENGTH, stdin); // get username
    del_newline(Name);
    print_member();
    printf(MSG_INPUT_ROOM);
    printf(MSG_PROMPT_PRE);
    fgets(room, NAME_LENGTH, stdin); // get roomname
    del_newline(room);


    /* prepare request for login */
    msg[0] = itoc(REQ_LOGIN);
    strcat(msg, Name);
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
            ;
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
        memset(Tmpbuf, 0, sizeof(Tmpbuf));

        /* get new message */
        // this version is for sudden receive message, prevent from replacing typing words.
        // in this version, the special control should implement by self
        getmsg();
        //fgets(msg, BUF_SIZE, stdin);      // get new message
        strcpy(msg, Tmpbuf);
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

        memset(tmp_name, 0, sizeof(Name));
        memset(buf, 0, sizeof(buf));
        memset(msg, 0, sizeof(msg));
        recv(socket_fd, buf, sizeof(buf), RECV_NORMAL);

        switch (buf[0]) {

            case '0': // system message

                strcpy(msg, &buf[1]);
                printf(MSG_TYPE0);      // erase line & print (replace current line)
                printf(MSG_PROMPT);     // restore user prompt
                printf("%s", Tmpbuf);   // restore the typing words
                break;

            case '1': // normal mode (1<username><message>)

                pch = strchr(buf, '\n');
                strcpyp(tmp_name, &buf[1], pch - 1); // take speaker's name
                strcpy(msg, pch + 1);
                printf(MSG_TYPE1);      // erase line & print (replace current line)
                printf(MSG_PROMPT);     // restore user prompt
                printf("%s", Tmpbuf);   // restore the typing words
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

/*
    Read one char without store in buffer.
    Input:  void
    Output: char
*/
char getch() {

    char buf = 0;
    struct termios old = {0};

    fflush(stdout);
    if (tcgetattr(0, &old) < 0) perror("tcsetattr()");

    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;

    if (tcsetattr(0, TCSANOW, &old) < 0) perror("tcsetattr ICANON");
    if (read(0, &buf,1) < 0) perror("read()");

    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;

    if (tcsetattr(0, TCSADRAIN, &old) < 0) perror ("tcsetattr ~ICANON");

    printf("%c",buf); // print read char
    return buf;
}

void getmsg() {

    int cur = 0, maxcur = 0, index = -1, beg_search = 0;
    char c;

    while(1) {

        printf(STORE_CUR);
        c = getch();                    // read each char and store it simultaneously

        if (c == 127) {                 // special control (backspace)
            
            cur--;
            maxcur--;
            if (cur < 0) cur = 0;
            if (maxcur < 0) maxcur = 0;

            if (cur != maxcur) {                        // if cursor not at bottom

                int j;
                printf(ERASE_LINE);
                printf(MSG_PROMPT);            
                         // restore user prompt

                for (j = maxcur; j > cur; j--) {        // erase char and concate char
                    Tmpbuf[j - 1] = Tmpbuf[j];
                }
                Tmpbuf[maxcur] = '\0';
                printf("%s", Tmpbuf);                   // restore typing words
                for (j = 1; j <= maxcur - cur; j++) {   // restore cursor position
                    printf(LEFT);
                }

            } else {                                    // if cursor bottom

                Tmpbuf[maxcur] = '\0';
                printf(ERASE_LINE""MSG_PROMPT);
                printf("%s", Tmpbuf);

            }
            strcpy(History[Num_History], Tmpbuf);

        } else if (c == 27) {           // special control (up down left right) [27 91 (same)| 65 66 68 67] total 3 char
            
            char c2 = 0, c3 = 0;
            c2 = getch();
            c3 = getch();
            if (c2 == 91) {
                if (c3 == 65 || c3 == 66) {                         // push up or down, cancel operation
                    if (c3 == 65 && beg_search == 0) {
                        beg_search = 1;
                        index = Num_History;
                    }
                    printf(RESTORE_CUR);
                    if (c3 == 65) {

                        index--;
                        if (index < 0) index = 0;
                        strcpy(Tmpbuf, History[index]);

                    } else {
                        
                        index++;
                        if (index > Num_History) {
                            index = Num_History;
                            memset(Tmpbuf, 0, BUF_SIZE);
                            strcpy(Tmpbuf, History[index]);
                            continue;
                        } else {
                            memset(Tmpbuf, 0, BUF_SIZE);
                            strcpy(Tmpbuf, History[index]);
                        }
                        
                    }
                    printf(ERASE_LINE);
                    printf(MSG_PROMPT);                             // restore user prompt
                    printf("%s", Tmpbuf);                   
                    maxcur = strlen(Tmpbuf);
                    cur = maxcur;
                }
                else if (c3 == 67 || c3 == 68) {
                    if (c3 == 68) {                                 // push left
                        if (cur - 1 < 0) printf(RESTORE_CUR);       // if cursor at start, cancel operation
                        else cur--;
                    } else {                                        // push right
                        if (cur == maxcur) printf(RESTORE_CUR);     // if cursor at bottom, cancel operation
                        else cur++;
                    }
                }
            }
        
        } else if (c == '\n') {
            break;
        } else {
            if (cur != maxcur) {                        // if cursor not at bottom (insert char)
                int j, buflen = strlen(Tmpbuf);
                printf(ERASE_LINE);
                printf(MSG_PROMPT);                     // restore user prompt

                for (j = buflen - 1; j >= cur; j--) {   // insert char
                    Tmpbuf[j + 1] = Tmpbuf[j];
                }
                Tmpbuf[cur] = c;
                printf("%s", Tmpbuf);                   // restore typing words
                for (j = 1; j <= maxcur - cur; j++) {   // restore cursor position
                    printf(LEFT);
                }

            } else {                                    // if cursor at bottom
                Tmpbuf[maxcur] = c;
            }
            cur++;
            maxcur++;
            strcpy(History[Num_History], Tmpbuf);
            beg_search = 0;
        }
    }

    if (beg_search == 1 || strcmp(Tmpbuf, "") == 0) {
        beg_search = 0;
    } else Num_History++;
    if (Num_History > HISTORY_SIZE) Num_History = HISTORY_SIZE;
}
