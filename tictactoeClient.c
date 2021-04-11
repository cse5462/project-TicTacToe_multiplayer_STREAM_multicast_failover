/**********************************************************/
/* This program is a 'pass and play' version of tictactoe */
/* Two users, player 1 and player 2, pass the game back   */
/* and forth, on a single computer                        */
/**********************************************************/

/* include files go here */
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
/* Define the number of rows and columns */
#define ROWS 3
#define COLUMNS 3
/* The number of command line arguments. */
#define NUM_ARGS 3

/* C language requires that you predefine all the routines you are writing */
struct buffer
{
    char version;
    char command;
    char data;
    char gameNumber;
};
int checkwin(char board[ROWS][COLUMNS]);
void print_board(char board[ROWS][COLUMNS]);
int tictactoe();
int initSharedState(char board[ROWS][COLUMNS]);
struct buffer P2choice(struct buffer player2);

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
    Buffer.version = 6;
    Buffer.command = 0;
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

int tictactoe(char board[ROWS][COLUMNS], int sd, struct sockaddr_in *serverAdd)
{
    /* this is the meat of the game, you'll look here for how to change it up */
    int player = 1; // keep track of whose turn it is
    int i, rc;      // used for keeping track of choice user makes
    int row, column;
    char mark, pick; // either an 'x' or an 'o'
    struct buffer player2, player1 = {0};
    struct buffer *p2Player1 = &player1;
    char gameNumber;
    /* loop, first print the board, then ask player 'n' to make a move */
    do
    {
        int choice;
        print_board(board);            // call function to print the board on the screen
        player = (player % 2) ? 1 : 2; // Mod math to figure out who the player is
        if (player == 2)
        {
            player2.gameNumber = gameNumber;
            player2 = P2choice(player2);
            pick = player2.data;
        }
        else
        {
            printf("Waiting for square selection from player 1..\n"); // gets chosen spot from player 1
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
            } while (rc < 4);
            pick = p2Player1->data;
            if (gameNumber == 0)
            {
                gameNumber = p2Player1->gameNumber;
            }

            if (rc <= 0)
            {
                printf("Connection lost!\n");
                printf("Closing connection!\n");
                exit(1);
            }
        }

         if (p2Player1->command == 0 || p2Player1->version != 6 || gameNumber != p2Player1->gameNumber)
            {
                printf("Player 1 sent invalid datagram\n");
                printf("Closing connection!\n");
                exit(1);
            }
        choice = pick - '0'; // converts to a int
        if (player == 1)     // prints choices
        {
            printf("Player 1 picked: %d\n", choice);
        }
        else
        {
            printf("Player 2 picked: %d\n", choice);
        }
        mark = (player == 1) ? 'X' : 'O'; //depending on who the player is, either us x or o
        /******************************************************************/
        /** little math here. you know the squares are numbered 1-9, but  */
        /* the program is using 3 rows and 3 columns. We have to do some  */
        /* simple math to conver a 1-9 to the right row/column            */
        /******************************************************************/
        row = (int)((choice - 1) / ROWS);
        column = (choice - 1) % COLUMNS;

        /* first check to see if the row/column chosen is has a digit in it, if it */
        /* square 8 has and '8' then it is a valid choice                          */

        if (board[row][column] == (choice + '0'))
        {
            board[row][column] = mark;
            // sends player 2 chioce if it is valid on the board
            if (player == 2)
            {
                printf("Sending normally...\n");
                printf("NormalSend Version: %d, Command %d, Data %c\n", player2.version, player2.command, player2.data);
                rc = send(sd, &player2, sizeof(player2), MSG_NOSIGNAL);
                if (rc < 0)
                {
                    printf("%d\n", rc);
                    printf("Connection lost!\n");
                    printf("Closing connection!\n");
                    printf("Bye\n");
                    exit(1);
                }
            }
        }
        else
        {
            printf("Invalid move\n");
            if (player == 1)
            {
                printf("The spot picked is not empty\n");
                printf("Closing the game & connection\n");
                exit(1);
            }
            else if (player == 2)
            {
                printf("The spot picked is not empty\n");
                printf("Pick a new number\n");
                continue;
            }
            player--;
            getchar();
        }
        /* after a move, check to see if someone won! (or if there is a draw */
        i = checkwin(board);
        printf("checking win\n");
        if (i != -1)
        {
            if (player == 1)
            {
                printf("Player 1 has signaled that the game has ended...\n");
                printf("Player 2 responding that it has got GAME_OVER from player 1...\n");
                player2.command = 2;

                printf("GameOverSend Version: %d, Command %d, Data %c\n", player2.version, player2.command, player2.data);
                rc = send(sd, &player2, sizeof(player2), MSG_NOSIGNAL);
                if (rc < 0)
                {
                    printf("%d\n", rc);
                    printf("Connection lost!\n");
                    printf("Closing connection!\n");
                    printf("Bye\n");
                    exit(1);
                }
            }
            else
            {

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
                } while (rc < 4);
                printf("Player 1 version: %d, Command: %d , Data: %c GameNumber %d \n", p2Player1->version, p2Player1->command, p2Player1->data, p2Player1->gameNumber);
                printf("Recived GAME_OVER\n");

                if (rc <= 0)
                {
                    printf("Connection lost!\n");
                    printf("Closing connection!\n");
                    exit(1);
                }
            }
        }

        player++;
        // memset(pick, 0, 1);

    } while (i == -1); // -1 means no one won

    /* print out the board again */
    print_board(board);

    if (i == 1) // means a player won!! congratulate them
        printf("==>\aPlayer %d wins\n ", --player);
    else
        printf("==>\aGame draw"); // ran out of squares, it is a draw

    return 0;
}

int checkwin(char board[ROWS][COLUMNS])
{
    /************************************************************************/
    /* brute force check to see if someone won, or if there is a draw       */
    /* return a 0 if the game is 'over' and return -1 if game should go on  */
    /************************************************************************/
    if (board[0][0] == board[0][1] && board[0][1] == board[0][2]) // row matches
        return 1;

    else if (board[1][0] == board[1][1] && board[1][1] == board[1][2]) // row matches
        return 1;

    else if (board[2][0] == board[2][1] && board[2][1] == board[2][2]) // row matches
        return 1;

    else if (board[0][0] == board[1][0] && board[1][0] == board[2][0]) // column
        return 1;

    else if (board[0][1] == board[1][1] && board[1][1] == board[2][1]) // column
        return 1;

    else if (board[0][2] == board[1][2] && board[1][2] == board[2][2]) // column
        return 1;

    else if (board[0][0] == board[1][1] && board[1][1] == board[2][2]) // diagonal
        return 1;

    else if (board[2][0] == board[1][1] && board[1][1] == board[0][2]) // diagonal
        return 1;

    else if (board[0][0] != '1' && board[0][1] != '2' && board[0][2] != '3' &&
             board[1][0] != '4' && board[1][1] != '5' && board[1][2] != '6' &&
             board[2][0] != '7' && board[2][1] != '8' && board[2][2] != '9')

        return 0; // Return of 0 means game over
    else
        return -1; // return of -1 means keep playing
}

void print_board(char board[ROWS][COLUMNS])
{
    /*****************************************************************/
    /* brute force print out the board and all the squares/values    */
    /*****************************************************************/

    printf("\n\n\n\tCurrent TicTacToe Game\n\n");

    printf("Player 1 (X)  -  Player 2 (O)\n\n\n");

    printf("     |     |     \n");
    printf("  %c  |  %c  |  %c \n", board[0][0], board[0][1], board[0][2]);

    printf("_____|_____|_____\n");
    printf("     |     |     \n");

    printf("  %c  |  %c  |  %c \n", board[1][0], board[1][1], board[1][2]);

    printf("_____|_____|_____\n");
    printf("     |     |     \n");

    printf("  %c  |  %c  |  %c \n", board[2][0], board[2][1], board[2][2]);

    printf("     |     |     \n\n");
}

int initSharedState(char board[ROWS][COLUMNS])
{
    /* this just initializing the shared state aka the board */
    int i, j, count = 1;
    printf("in sharedstate area\n");
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
        {
            board[i][j] = count + '0';
            count++;
        }

    return 0;
}
struct buffer P2choice(struct buffer player2)
{

    int input;
    printf("Player 2, enter a number:  "); // player 2 picks a spot
    scanf("%d", &input);                   //using scanf to get the choice
    while (getchar() != '\n')
        ;
    while (input < 1 || input > 9) //makes sure the input is between 1-9
    {
        printf("Invalid input choose a number between 1-9.\n");
        printf("Player 2, enter a number:  "); // player 2 picks a spot

        scanf("%d", &input);
        while (getchar() != '\n')
            ;
    }
    player2.version = 6;
    player2.command = 1;
    player2.data = input + '0';
    return player2;
}
