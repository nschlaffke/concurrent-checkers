//
// Created by ns on 29.12.16.
//

#ifndef PW_STRUCTS_H
#define PW_STRUCTS_H

#include <signal.h>

#include <sys/types.h> // kolejki komunikatow
#include <sys/ipc.h> // -||-
#include <sys/msg.h>// -||-
#include <unistd.h> // getpid()
#include <errno.h> // obsluga bledow
#include <sys/sem.h> // semafory
#include <sys/shm.h> // shared memmory
#include <sys/wait.h>
#include <malloc.h>
#define DEBUG printf("%d\n", __LINE__)
#define ERROR(x, komunikat) if(x == -1) {printf("LINE %d: ", __LINE__); perror(komunikat); /*return 0*/;}
#define ERROR_R(x, komunikat) if(x == -1) {printf("USER %s LEFT\n", (*wsk).nick); if(who == 1){zbite_c = 12;}else{zbite_b = 12;} left = 1;/*return 0*/;}
#define ERRORP(x, komunikat) if(x == (void *)-1) {printf("%d", __LINE__); perror(komunikat); /*return 0*/;}
#define KILL(x) if(kill(x, 0) != -1) {kill(x, SIGTERM);}
#define MAIN_Q_NR ftok("serwer.c", 1)
#define W_PLAY 6
#define PID 8
#define BOARD_ID 7
#define RUCH 5
#define ACCEPTED 1
#define DECLINED 0
#define NOT_EXIST 2
#define CLEAR_STDOUT printf("\033[2J\033[1;1H")
#define INVITATION_S 3
#define INVITATION_R 4
#define TEXT_MESSAGE 2
#define PID_AND_NICK 1
#define ACTUAL_SIZE(X) sizeof(X) - sizeof(long)
#define MAX_SIZE 50
#define MAX_PLAYERS 10
#define WRONG_MOVEMENT 4
#define YOUR_TURN 1
#define YOU_WON 2
#define YOU_LOST 3
#define TRUE 1
#define FALSE 0
typedef struct
{
    long mtype;
    pid_t pid;
    char nick[MAX_SIZE];
} msgbuf;

typedef struct
{
    long mtype;
    pid_t pid;
} pid_m;

typedef struct
{
    long mtype;
    char sender[MAX_SIZE];
    char msg[MAX_SIZE];
} text_message;

typedef struct
{
    char nick[MAX_SIZE];
    pid_t pid;
    int q_id;
    int is_online;
} player;

typedef struct
{
    long mtype;
    char sender[MAX_SIZE];
    char invited[MAX_SIZE];
    pid_t pid;
} invitation;

typedef struct
{
    long mtype;
    int a;
} bool;

typedef struct
{
    long mtype;
    int shm_id;
    int sem_id;
} board_ids;
typedef struct
{
    long mtype;
    char c1;
    char c2;
    int r1;
    int r2;

}ruch;
struct sembuf sem_inc = {0, 1, 0};
struct sembuf sem_dec = {0, -1, 0};

int semaphor()
{
    int id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    semctl(id, 0, SETVAL, (short) 1);
    return id;
}

typedef enum
{
    bialy,
    bialy_d,
    czarny,
    czarny_d,
    pusty,
} state;
#endif //PW_STRUCTS_H
