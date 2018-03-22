//
// Created by ns on 29.12.16.
//
#include <stdio.h>
#include <string.h>
#include <sys/shm.h> // pamiec wspoldzielona
#include "header.h"
#include <stdlib.h>
#define W_PLANSZY(A) (A)>=0 && (A)<8
#define DOZWOLONY(a, b, c, d) (abs(a-c) == abs(b-d))
#define GORA(b, d) (b > d)
#define PRAWO(a, c) (a < c)
state przeciwnik (state gracz);
void konwertuj(char *a, int *b, char *c, int *d);
void initzialize_board(state **wsk);
int wykonaj_ruch(int a, int b, int c, int d, state **board, state gracz, unsigned int *zbite_b, unsigned int *zbite_c);
int main()
{
    int p_mmry_id = shmget(IPC_PRIVATE, sizeof(player) * MAX_PLAYERS, IPC_CREAT | 0600); // pamiec z tablica graczy
    ERROR(p_mmry_id, "TWORZENIE PAMIECI WSPOLDZIELONEJ PLAYERS");
    int counter_mmry_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600);
    int p_sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600); // tworzymy semafor
    ERROR(p_sem_id, "SEMGET P_SEM_ID");
    ERROR(semctl(p_sem_id, 0, SETVAL, (short) 1), "P_SEM_ID SEMCTL"); // ustawiamy wartosc poczatkowa semafora
    int m_q_id = msgget(MAIN_Q_NR, IPC_CREAT | 0600);
    ERROR(m_q_id, "M_Q_ID, MSGSET:");
    pid_t new_players;
    if ((new_players = fork()) == 0) // tu dodawani sa nowi gracze
    {
        player *players = shmat(p_mmry_id, NULL, 0);
        ERRORP(players, "SHMAT NEW PLAYERS");
        int *counter = shmat(counter_mmry_id, NULL, 0);
        ERRORP(counter, "SHMAT NEW_PLAYERS");
        msgbuf tmp;
        while (1)
        {
            ERROR(msgrcv(m_q_id, &tmp, ACTUAL_SIZE(msgbuf), PID_AND_NICK, 0), "PLAYERS MSGRCV:");

            ERROR(semop(p_sem_id, &sem_dec, 1), "P_SEM_ID OPUSZCZANIE");
            printf("USER %s JOINED\n", tmp.nick);
            players[*counter].pid = tmp.pid;
            strcpy(players[*counter].nick, tmp.nick);
            players[*counter].is_online = 1;
            players[*counter].q_id = msgget(tmp.pid, IPC_CREAT);
            ERROR(players[*counter].q_id, "PRYWATNA KOLEJKA");
            pid_m pid_tmp;
            pid_tmp.pid = getpid();
            pid_tmp.mtype = PID;
            msgsnd(players[*counter].q_id, &pid_tmp, ACTUAL_SIZE(pid_m), 0); // wysylamy pid serwera do klienta
            (*counter)++;
            ERROR(semop(p_sem_id, &sem_inc, 1), "P_SEM_ID PODNOSZENIE");
        }
    }
    pid_t sender;
    if ((sender = fork()) == 0) // tu wysylane sa wiadomosci w lobby
    {
        int *counter = shmat(counter_mmry_id, NULL, 0);
        ERRORP(counter, "counter sender");
        player *players = shmat(p_mmry_id, NULL, 0);
        ERRORP(players, "SHMAT NEW PLAYERS");
        text_message tmp;
        while (1)
        {
            ERROR(msgrcv(m_q_id, &tmp, ACTUAL_SIZE(text_message), TEXT_MESSAGE, 0), "ODBIOR WIADOMOSCI");
            int i;
            for (i = 0; i < *counter; i++)
            {
                ERROR(semop(p_sem_id, &sem_dec, 1), "P_SEM_ID OPUSZCZANIE");
                if (players[i].is_online)
                {
                    if (kill(players[i].pid, 0) != -1)
                        ERROR(msgsnd(players[i].q_id, &tmp, ACTUAL_SIZE(tmp), 0), "WIADOMOSC PRYWATNA");
                }
                ERROR(semop(p_sem_id, &sem_inc, 1), "P_SEM_ID PODNOSZENIE");
            }

        }
    }
    pid_t games;
    if ((games = fork()) == 0) // tu prowadzone sa rozgrywki
    {
        int *counter = shmat(counter_mmry_id, NULL, 0);
        ERRORP(counter, "COUNTER SHMAT");
        player *players = shmat(p_mmry_id, NULL, 0);
        ERRORP(players, "PLAYERS SHMAT");
        while(1)
        {
            int i;
            for(i = 0; i < *counter; i++)
            {
                if(players[i].is_online == 1)
                {
                    if(kill(players[i].pid, 0) != -1)
                    {
                        invitation tmp;
                        ssize_t result;
                        result = msgrcv(players[i].q_id, &tmp, ACTUAL_SIZE(invitation), INVITATION_S, IPC_NOWAIT);
                        if(result != -1 && fork() == 0)
                        {
                            printf("INVITATION FROM %s TO %s\n", players[i].nick, tmp.invited);
                            // przyszlo zaproszenie, przesylamy do adresata
                            int k;
                            bool w_play;
                            for(k = 0; k < *counter; k++) // players[i] zaprasza players[k]
                            {
                                if(players[k].is_online == 1 && !strcmp(players[k].nick, tmp.invited))
                                {
                                    // sprawdzamy czy zapraszany chce grac
                                    tmp.mtype = INVITATION_R;
                                    ERROR(msgsnd(players[k].q_id,  &tmp, ACTUAL_SIZE(invitation), 0), "MSGSND PLAYERS");
                                    printf("%s INVITED\n", players[k].nick);

                                    ERROR(msgrcv(players[k].q_id, &w_play, ACTUAL_SIZE(w_play), W_PLAY, 0), "MSGRCV W_PLAY");
                                    if(w_play.a == 1)
                                    {
                                        ERROR(msgsnd(players[i].q_id, &w_play, ACTUAL_SIZE(w_play), 0), "MSGRCV W_PLAY");
                                        // rozpoczyna się rozgrywka dwóch graczy
                                        player *white = &players[i];
                                        player *black = &players[k];
                                        state **board;
                                        state *board_data;
                                        int board_data_id = shmget(IPC_PRIVATE, 8 * 8 * sizeof(state),  IPC_CREAT | 0600);
                                        int board_sem = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
                                        ERROR(board_sem, "BOARD_SEM SEMGET");
                                        ERROR(board_data_id, "SHMGET BOARD DATA ID");
                                        semctl(board_sem, 0, SETVAL, 1);
                                        board_data = shmat(board_data_id, NULL, 0);
                                        ERRORP(board_data, "SHMAT");
                                        board = malloc(sizeof(board[0]) * 8);
                                        ERRORP(board, "malloc\n");
                                        int l;
                                        for(l = 0; l < 8; l++)
                                            board[l] = board_data + 8 * l;
                                        initzialize_board(board);
                                        board_ids msg;
                                        msg.mtype = BOARD_ID;
                                        msg.shm_id = board_data_id;
                                        msg.sem_id = board_sem;
                                        msgsnd(white->q_id, &msg, ACTUAL_SIZE(board_ids), 0);
                                        msgsnd(black->q_id, &msg, ACTUAL_SIZE(board_ids), 0);

                                        initzialize_board(board);
                                        printf("GAME STARTED: %s vs %s\n", players[k].nick, players[i].nick);
                                        unsigned int zbite_b = 0,
                                                zbite_c = 0;
                                        int wynik;
                                        int who = 1;
                                        while(zbite_b < 12 && zbite_c < 12)
                                        {
                                            char a, c;
                                            int b, d;
                                            player *wsk, *op;
                                            state gracz;
                                            if(who == 1)
                                            {
                                                wsk = white;
                                                op = black;
                                                gracz = bialy;
                                            }
                                            else
                                            {
                                                wsk = black;
                                                op = white;
                                                gracz = czarny;
                                            }
                                            bool tmp;
                                            tmp.mtype = W_PLAY;
                                            tmp.a = 1;
                                            int left = 0;
                                            ERROR_R(msgsnd((*wsk).q_id, &tmp, ACTUAL_SIZE(bool), 0), "MSGSND TMP:");
                                            do
                                            {
                                                printf("%s MOVES...\n", (*wsk).nick);
                                                // przyjmujemy ruch
                                                ruch r;
                                                ERROR_R(msgrcv((*wsk).q_id, &r, ACTUAL_SIZE(ruch), RUCH, 0), "BIALY RUCH");
                                                printf("MOVEMENT RECIEVED (%s)\n", (*wsk).nick);
                                                a = r.c1;
                                                c = r.c2;
                                                b = r.r1;
                                                d = r.r2;
                                                konwertuj(&a, &b, &c, &d);
                                                ERROR(semop(board_sem, &sem_dec, 1), "board_sem SEMOP");
                                                wynik = wykonaj_ruch(a, b, c, d, board, gracz, &zbite_b, &zbite_c);
                                                ERROR(semop(board_sem, &sem_inc,1), "board_sem SEMOP");
                                                if(!wynik && op->is_online)
                                                {
                                                    printf("INCORRECT MOVEMENT\n");
                                                    tmp.a = 4;
                                                    ERROR_R(msgsnd((*wsk).q_id, &tmp, ACTUAL_SIZE(bool), 0), "MSGSND TMP:");
                                                }
                                                else
                                                {
                                                    tmp.a = 1;
                                                    ERROR_R(msgsnd((*wsk).q_id, &tmp, ACTUAL_SIZE(bool), 0), "MSGSND TMP:");
                                                }
                                            }while(wynik != 1 && !left && op->is_online);
                                            who = !who;
                                        }
                                        // gra skonczona
                                        player winner, looser;
                                        bool tmp;
                                        if(zbite_c == 12)
                                        {
                                            winner = *black;
                                            looser = *white;
                                        }
                                        else
                                        {
                                            winner = *white;
                                            looser = *black;
                                        }
                                        tmp.mtype = W_PLAY;
                                        tmp.a = YOU_WON;
                                        msgsnd(winner.q_id, &tmp, ACTUAL_SIZE(bool), 0);
                                        tmp.a = YOU_LOST;
                                        msgsnd(looser.q_id, &tmp, ACTUAL_SIZE(bool), 0);
                                        free(board);
                                        shmdt(board_data);
                                        ERROR(shmctl(board_data_id, IPC_RMID, 0), "BOARD_ID SHMCTL RM");
                                        ERROR(semctl(board_sem, IPC_RMID, 0), "board_sem RM");
                                    }
                                    else
                                    {
                                        w_play.a = 0;
                                        printf("%s WON'T PLAY\n", players[i].nick);
                                        ERROR(msgsnd(players[i].q_id, &w_play, ACTUAL_SIZE(w_play), 0), "MSGRCV W_PLAY");
                                        break;
                                    }
                                }
                            }
                            if(w_play.a != DECLINED && w_play.a != ACCEPTED)
                            {
                                w_play.mtype = W_PLAY;
                                w_play.a = NOT_EXIST;
                                printf("%s DOESN'T EXIST\n", tmp.invited);
                                ERROR(msgsnd(players[i].q_id, &w_play, ACTUAL_SIZE(w_play), 0), "MSGRCV W_PLAY");
                            }
                            else
                                printf("GAME %s vs %s ENDS\n", tmp.invited, players[i].nick);
                            _exit(0);
                        }
                    }
                    else
                    {
                        printf("PLAYER %s LEFT\n", players[i].nick);
                        ERROR(semop(p_sem_id, &sem_dec, 1), "P_SEM_ID OPUSZCZANIE");
                        players[i].is_online = 0;
                        ERROR(semop(p_sem_id, &sem_inc, 1), "P_SEM_ID PODNOSZENIE");
                        msgctl(players[i].q_id, IPC_RMID, 0);
                    }
                }
            }
        }
    }
    int cleaner;
    if((cleaner = fork()) == 0) // tu wylaczymy serwer
    {
        while(1)
        {
            printf("By wylaczyc serwer wpisz 0:\n");
            int x;
            scanf("%d", &x);
            if(x == 0)
            {
                // tutaj czyscimy
                KILL(games);
                KILL(new_players);
                KILL(sender);
                int *counter = shmat(counter_mmry_id, NULL, 0);
                player *players = shmat(p_mmry_id, NULL, 0);
                int i;
                for(i = 0; i < *counter; i++)
                {
                    if(players[i].is_online == 1)
                        msgctl(players[i].q_id, IPC_RMID, 0);
                }
                shmctl(p_mmry_id, IPC_RMID, 0); // tablica graczy
                shmctl(counter_mmry_id, IPC_RMID, 0); // licznik graczy
                semctl(p_sem_id, IPC_RMID, 0);
                msgctl(m_q_id, IPC_RMID, 0); // usuwamy kolejke glowna
                _exit(0);
            }
        }
    }
    waitpid(new_players, 0, 0);
    waitpid(sender, 0, 0);
    waitpid(games, 0, 0);
    waitpid(cleaner, 0, 0);
}
void initzialize_board(state **wsk)
{
    state tmp[8][8] =
            {
            {pusty, czarny,pusty,czarny,pusty, czarny, pusty, czarny},
            {czarny,pusty,czarny,pusty, czarny, pusty, czarny, pusty},
            {pusty,czarny,pusty, czarny, pusty, czarny, pusty,czarny},
            {pusty, pusty, pusty, pusty, pusty, pusty , pusty, pusty},
            {pusty, pusty, pusty, pusty, pusty, pusty , pusty, pusty},
            {bialy,pusty,bialy,pusty, bialy, pusty, bialy, pusty},
            {pusty, bialy,pusty,bialy,pusty, bialy, pusty, bialy},
            {bialy,pusty,bialy,pusty, bialy, pusty, bialy, pusty},
            };
    int i, k;
    for(i = 0; i < 8; i++)
    {
        for(k = 0; k < 8; k++)
        {
            wsk[i][k] = tmp[i][k];
        }
    }
}
int wykonaj_ruch(int a, int b, int c, int d, state **board, state gracz, unsigned int *zbite_b, unsigned int *zbite_c)
{
    if (!(DOZWOLONY(a, b, c, d) && W_PLANSZY(a) && W_PLANSZY(b) && W_PLANSZY(c) && W_PLANSZY(d)))
        return 0;
        //zly start
    else if (board[b][a] == pusty || (board[b][a] != gracz && board[b][a] != gracz+1)) // nie ma pionka lub nie nasz pionek
        return 0;
        //zly koniec
    else if (board[d][c] != pusty) // pole zajete
        return 0;
    else if (board[b][a] == gracz+1) // ruszamy damka
    {
        int tmp_a = a, tmp_b = b;
        unsigned int wrog = przeciwnik(gracz);
        do
        {
            if (GORA(b, d))
                b--;
            else
                b++;
            if (PRAWO(a, c))
                a++;
            else
                a--;
            if (board[b][a]!= pusty && (a != c && b != d))
            {
                if ((board[b][a] == wrog || board[b][a] == wrog + 1)  && abs(a-c) ==  1 && abs(b-d) == 1)
                {
                    board[b][a] = pusty;
                    if (gracz == czarny)
                        (*zbite_b)++;
                    else
                        (*zbite_c)++;
                    break;
                }
                else
                    return 0;
            }
        }
        while(a != c && b != d);
        board[tmp_b][tmp_a] = pusty;
        board[d][c] = gracz + 1;
        return 1;
    }
    else // ruch zwyklym pionkiem
    {
        int tmp_a = a, tmp_b = b;
        unsigned int wrog = przeciwnik(gracz);
        if (GORA(b, d))
            b--;
        else
            b++;
        if (PRAWO(a, c))
            a++;
        else
            a--;
        if ((!GORA(tmp_b, d) && gracz == bialy) || (GORA(tmp_b,d) && gracz == czarny)) // idziemy w dol - musimy bic
        {
            if (board[b][a] != wrog && board[b][a] != wrog + 1 && !(abs(a-c) ==  1 && abs(b-d) == 1)) // oszukujemy
                return 0;
            else
                board[b][a] = pusty;
        }
        else if (board[b][a]!= pusty && (a != c && b != d)) // idziemy w gore
        {
            if ((board[b][a] == wrog || board[b][a] == wrog+1) && abs(a-c) ==  1 && abs(b-d) == 1) // konczymy ruch
            {
                board[b][a] = pusty; // bijemy pionka
                if (gracz == czarny)
                    (*zbite_b)++;
                else
                    (*zbite_c)++;
            }
            else
                return 0;
        }
        board[tmp_b][tmp_a] = pusty;
        if ((d == 0 && gracz == bialy) || (d == 7 && gracz == czarny)) // dotarlismy na koniec planszy
            board[d][c] = gracz + 1; // mamy damke
        else
            board[d][c] = gracz;
        return 1;
    }
}
void konwertuj(char *a, int *b, char *c, int *d)
{
    *a -= 'a';
    *c -= 'a';
    if(*b != 8)
    {
        *b = abs(*b-7);
        (*b)++;
    }
    else
        *b = 0;
    if(*d != 8)
    {
        *d = abs(*d-7);
        (*d)++;
    }
    else
        *d = 0;
}
state przeciwnik (state gracz)
{
    if(gracz == bialy || gracz == bialy_d)
        return czarny;
    else
        return bialy;
}
