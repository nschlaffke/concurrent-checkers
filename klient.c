//
// Created by ns on 30.12.16.
//
#include <stdio.h>

#include "header.h"
#include <string.h>

void drukuj_plansze(state **plansza);
state **board; // do obslugi planszy
void leave_and_free(int x)
{
    free(board);
    _exit(0);
}
int main()
{
    pid_m server_pid; // pobranie pidu po utworzeniu kolejki prywatnej
    signal(SIGINT, leave_and_free);
    state *board_data = 0; // -||-
    int board_data_id = 0, board_sem_id = 0;
    msgbuf msg;
    msg.mtype = PID_AND_NICK;
    msg.pid = getpid();
    pid_t pid = msg.pid;
    printf("Podaj nick:\n");
    char nick[50];
    scanf("%s", nick); // UWAGA NA DLUGOSC!
    strcpy(msg.nick, nick);
    int m_q_id = msgget(MAIN_Q_NR, IPC_CREAT | 0600);
    int msg_q_id = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    int flag_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0600);
    ERROR(flag_id, "SHMGET FLAG");
    int *flag = shmat(flag_id, NULL, 0); // wylaczamy przychodzenie wiadomosci i zaproszen podczas rozgrywki
    ERRORP(flag, "SHMAT FLAG");
    *flag = 0;
    ERROR(m_q_id, "TWORZENIE KOLEJKI GLOWNEJ"); // kolejka glowna
    ERROR(msgsnd(m_q_id, &msg, ACTUAL_SIZE(msgbuf), 0), "PID_T MSGSND");
    int private_q_id = msgget((key_t) pid, IPC_CREAT | 0600);
    ERROR(private_q_id, "TWORZENIE KOLEJKI PRYWATNEJ");
    printf("Laczenie z serwerem...\n");
    ERROR(msgrcv(private_q_id, &server_pid, ACTUAL_SIZE(pid_m), PID, 0), "SERVER_PID MSGRCV");
    pid_t receiver;
    if ((receiver = fork()) == 0)
    {
        int *flag = shmat(flag_id, NULL, 0);
        ERRORP(flag, "SHMAT FLAG");
        while (1)
        {
            while(*flag);
            invitation tmp;
            tmp.mtype = INVITATION_S;
            text_message msg;
            msg.mtype = TEXT_MESSAGE;
            if (msgrcv(private_q_id, &tmp, ACTUAL_SIZE(invitation), INVITATION_R, IPC_NOWAIT) != -1)
            {
                if(strcmp(nick, tmp.sender) != 0)
                {
                    printf("<<< Zaproszenie od %s >>>\n< ", tmp.sender);
                    fflush(stdout);
                    ERROR(msgsnd(msg_q_id, &tmp, ACTUAL_SIZE(invitation), 0), "INVITATION MSGSND");
                }
            }
            while (msgrcv(private_q_id, &msg, ACTUAL_SIZE(text_message), TEXT_MESSAGE, IPC_NOWAIT) != -1)
            {
                if(strcmp(nick, msg.sender) != 0)
                {
                    printf("<<< Nowe wiadomosci >>>\n< ");
                    fflush(stdout);
                }
                ERROR(msgsnd(msg_q_id, &msg, ACTUAL_SIZE(text_message), 0), "INVITATION MSGSND");
            }
        }
    }
    if (fork() == 0) // czysciciel
    {
        signal(SIGINT, SIG_IGN);
        pid_t parent = getppid();
        while (1)
        {
            if (kill(server_pid.pid, 0) == -1 || kill(parent, 0) == -1) // jezeli serwer jest wylaczony lub parent jest wylaczony
            {
                kill(receiver, SIGTERM);
                if (kill(server_pid.pid, 0) == -1)
                    printf("\nProces serwera:\n");
                if (kill(parent, 0) != -1)
                    kill(parent, SIGTERM);
                // czyscimy wszystko
                /*msgctl(m_q_id, IPC_RMID, 0);*/ // usuwamy kolejke glowna !!! serwer usuwa
                /*msgctl(private_q_id, IPC_RMID, 0);*/ // usuwamy kolejke prywatna !!! serwer usuwa
                msgctl(msg_q_id, IPC_RMID, 0);
                shmctl(board_data_id, IPC_RMID, 0);
                shmctl(flag_id, IPC_RMID, 0);
                _exit(0);
            }
        }
    }
    printf("1. Wyslij wiadomosc 2. Zapros do gry 3. Odbierz wiadomosci 4. Odbierz Zaproszenia 5. Wyjscie\n");
    int game = 0;
    while (1)
    {
        if (game)
        {
            *flag = 1;
            printf("Gra rozpoczela sie\n");
            board_ids wsk;
            ERROR(msgrcv(private_q_id, &wsk, ACTUAL_SIZE(board_ids), BOARD_ID, 0), "BOARD_ID MSGRCV");
            board_data_id = wsk.shm_id;
            board_data = shmat(board_data_id, NULL, 0);
            board_sem_id = wsk.sem_id;
            board = malloc(sizeof(board[0]) * 8);
            int l;
            for (l = 0; l < 8; l++)
                board[l] = board_data + 8 * l;
            int end = FALSE;
            while (!end)
            {
                CLEAR_STDOUT;
                ERROR(semop(board_sem_id, &sem_dec, 1), "board_sem SEMOP");
                drukuj_plansze(board);
                ERROR(semop(board_sem_id, &sem_inc, 1), "board_sem SEMOP");
                bool msg;
                msgrcv(private_q_id, &msg, ACTUAL_SIZE(bool), W_PLAY, 0);
                if (msg.a == 1)
                {
                    CLEAR_STDOUT;
                    ERROR(semop(board_sem_id, &sem_dec, 1), "board_sem SEMOP");
                    drukuj_plansze(board);
                    ERROR(semop(board_sem_id, &sem_inc, 1), "board_sem SEMOP");
                    do
                    {
                        if (msg.a == 4)
                            printf("Bledny ruch:\n");
                        char a, c;
                        int b, d;
                        printf("Podaj ruch (np. a1-b2): ");
                        scanf(" %c%d-%c%d", &a, &b, &c, &d);
                        ruch tmp;
                        tmp.mtype = RUCH;
                        tmp.c1 = a;
                        tmp.c2 = c;
                        tmp.r1 = b;
                        tmp.r2 = d;
                        msgsnd(private_q_id, &tmp, ACTUAL_SIZE(ruch), 0);
                        msgrcv(private_q_id, &msg, ACTUAL_SIZE(bool), W_PLAY, 0);
                    } while (msg.a == 4);
                } else if (msg.a == 2)
                {
                    CLEAR_STDOUT;
                    printf("Wygrales\n");
                    printf("1. Wyslij wiadomosc 2. Zapros do gry 3. Odbierz wiadomosci 4. Odbierz Zaproszenia 5. Wyjscie\n");
                    break;
                } else if (msg.a == 3)
                {
                    CLEAR_STDOUT;
                    printf("Przegrales\n");
                    printf("1. Wyslij wiadomosc 2. Zapros do gry 3. Odbierz wiadomosci 4. Odbierz Zaproszenia 5. Wyjscie\n");
                    break;
                }
            }
            free(board);
            shmdt(board_data);
            board = 0;
            *flag = 0;
            game = 0;
        }
        int k;
        printf("< ");
        scanf("%d", &k);
        switch (k)
        {
            case 1:
            {
                text_message tmp;
                tmp.mtype = TEXT_MESSAGE;
                strcpy(tmp.sender, nick);
                printf("Podaj tresc:\n< ");
                while (getchar() != '\n');
                scanf("%[^\n]", tmp.msg);
                ERROR(msgsnd(m_q_id, &tmp, ACTUAL_SIZE(text_message), 0), "WYSYLANIE TEKSTU");
            }
                break;
            case 2:
            {
                // wysylanie zaproszen
                invitation tmp;
                tmp.mtype = INVITATION_S;
                strcpy(tmp.sender, nick);
                printf("Z kim chcesz grac?\n< ");
                scanf("%s", tmp.invited);
                if(strcmp(tmp.invited, nick) == 0)
                {
                    printf("Nie mozesz zaprosic siebie...\n");
                    break;
                }
                tmp.pid = pid;
                ERROR(msgsnd(private_q_id, &tmp, ACTUAL_SIZE(invitation), 0), "INVITATION MSGSND");
                printf("Oczekiwanie na odpowiedz...\n");
                bool w_play;
                ERROR(msgrcv(private_q_id, &w_play, ACTUAL_SIZE(bool), W_PLAY, 0), "INVITATION MSGRCV");
                if (w_play.a == ACCEPTED)
                    game = 1;
                else if (w_play.a == DECLINED)
                    printf("Zaproszenie odrzucone\n");
                else
                    printf("Gracz %s nie istnieje\n", tmp.invited);
            }
                break;
            case 3:
            {
                // wczytywanie wiadomosci
                text_message tmp;
                while (msgrcv(msg_q_id, &tmp, ACTUAL_SIZE(text_message), TEXT_MESSAGE, IPC_NOWAIT) != -1)
                    printf("%s: %s\n", tmp.sender, tmp.msg);
            }
                break;
            case 4:
            {
                // odbieranie zaproszen
                invitation tmp;
                if (msgrcv(msg_q_id, &tmp, ACTUAL_SIZE(invitation), INVITATION_R, IPC_NOWAIT) != -1)
                {
                    printf("Czy chcesz grac z %s? (1/0)\n", tmp.sender);
                    int p;
                    scanf("%d", &p);
                    bool b_tmp;
                    b_tmp.mtype = W_PLAY;
                    if (p == TRUE)
                    {
                        b_tmp.a = TRUE;
                        ERROR(msgsnd(private_q_id, &b_tmp, ACTUAL_SIZE(bool), 0), "WANT TO PLAY MESSAGE");
                        game = TRUE;
                    } else
                    {
                        b_tmp.a = FALSE;
                        ERROR(msgsnd(private_q_id, &b_tmp, ACTUAL_SIZE(bool), 0), "DON'T WANT TO PLAY MESSAGE");
                    }
                } else
                    printf("Brak zaproszen\n");
            }
                break;
            case 5:
                // wyjscie
                _exit(0);
            default:
                printf("BLEDNE POLECENIE\n");
                break;
        }
    }
}

void drukuj_plansze(state **plansza)
{
    int i, j;
    for (i = 0; i < 8; i++)
    {
        printf("%d|", 8 - i);
        for (j = 0; j < 8; j++)
        {
            if (plansza[i][j] == bialy)
                printf("O");
            else if (plansza[i][j] == czarny)
                printf("X");
            else if (plansza[i][j] == czarny_d)
                printf("*");
            else if (plansza[i][j] == bialy_d)
                printf("8");
            else
                printf(" ");
        }
        printf("\n");
    }
    printf("----------\n");
    printf("  ABCDEFGH\n");
}
