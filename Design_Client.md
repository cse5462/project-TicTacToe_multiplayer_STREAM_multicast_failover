# TicTacToe Client (Player 2) Design
> This is the design document for the TicTacToe Client ([tictactoeClient.c](https://github.com/CSE-5462-Spring-2021/assignment7-conner-ben/blob/main/tictactoeClient.c)).  
> By: Ben Nagel

## Table of Contents
- TicTacToe Class Protocol - [Protocol Document](https://docs.google.com/document/d/1TdURyFGCPU5t9XKMJgmWjt4Qqg4z8tZyW1bvMw0EmvI/edit?usp=sharing)
- [Environment Constants](#environment-constants)
- [High-Level Architecture](#high-level-architecture)
- [Low-Level Architecturet](#low-level-architecture)

## Environment Constants
```C#
/* Define the number of rows and columns */
#define ROWS 3 
#define COLUMNS 3
/* The number of command line arguments. */
#define NUM_ARGS 3
```

## High-Level Architecture
At a high level, the client application takes in input from the user and trys to connect to the server. If everything worked, it waits for the server to send the first move of tictactoe. Once the move was received, the client marks the move on the client board, if the move was valid, otherwise closes the connection. If the move was valid the client sends the server the next move and this processes continues until there is a winner or a tie.
```C
int main(int argc, char *argv[])
{
    struct buffer Buffer = {0};
    char board[ROWS][COLUMNS];
    int sd;
    struct sockaddr_in server_address;
    int portNumber;
    char serverIP[29];

    // check for two arguments
    if (argc != 3)
    {
        printf("Wrong number of command line arguments");
        printf("Input is as follows: tictactoeP2 <ip-address> <port-num>");
        exit(1);
    }
    // create the socket
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd < 0)
    {
        printf("ERROR making the socket");
        exit(1);
    }
    else
    {
        printf("Socket Created\n");
    }
    portNumber = strtol(argv[2], NULL, 10);
    strcpy(serverIP, argv[1]);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(portNumber);
    server_address.sin_addr.s_addr = inet_addr(serverIP);
    // connnect to the sever
    if (connect(sd, (struct sockaddr *)&server_address, sizeof(struct sockaddr_in)) < 0)
    {
        close(sd);
        perror("error	connecting	stream	socket");
        exit(1);
    }
    printf("Connected to the server!\n");
    Buffer.version = 5;
    Buffer.command = 0;
    Buffer.seqNum = 0;
    Buffer.data = 0;
    printf("Attempting to send game request...\n");
    if (send(sd, &Buffer, sizeof(Buffer), MSG_NOSIGNAL) <= 0)
    {
        close(sd);
        perror("error    connecting    stream    socket");
        exit(1);
    }
    initSharedState(board);                                   // Initialize the 'game' board
    tictactoe(board, sd, (struct sockaddr *)&server_address); // call the 'game'
    return 0;
}
```

## Low-Level Architecture
- Client wins and waits for GAME_OVER COMMAND
```C
 printf("Waiting for player 1 to issue a GAME_OVER command...\n");
                rc = 0;
                do
                {
                    rc += recv(sd, p2Player1 + rc, sizeof(player1), 0);
                     if (rc <= 0)
                {
                    printf("Connection lost!\n");
                    printf("Closing connection!\n");
                    exit(1);
                }
                } while (rc < 5);
                printf("Player 1 version: %d , SeqNum: %d , Command: %d , Data: %c GameNumber %d \n", p2Player1->version, p2Player1->seqNum, p2Player1->command, p2Player1->data, p2Player1->gameNumber);
                printf("Recived GAME_OVER\n");

                if (rc <= 0)
                {
                    printf("Connection lost!\n");
                    printf("Closing connection!\n");
                    exit(1);
                }

```
- Server wins and waits for a GAME_OVER COMMAND from the client
```C
 printf("Player 1 has signaled that the game has ended...\n");
                printf("Player 2 responding that it has got GAME_OVER from player 1...\n");
                player2.seqNum++;
                player2.command = 2;

                printf("GameOverSend Version: %d , SeqNumber %d , Command %d, Data %c\n", player2.version, player2.seqNum, player2.command, player2.data);
                rc = send(sd, &player2, sizeof(player2), MSG_NOSIGNAL);
                if (rc < 0)
                {
                    printf("%d\n", rc);
                    printf("Connection lost!\n");
                    printf("Closing connection!\n");
                    printf("Bye\n");
                    exit(1);
                }
```

