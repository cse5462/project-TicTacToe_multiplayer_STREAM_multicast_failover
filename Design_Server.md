# TicTacToe Server (Player 1) Design
> This is the design document for the TicTacToe Server ([tictactoeServer.c](https://github.com/CSE-5462-Spring-2021/project-1-conner-ben/blob/main/tictactoeServer.c)).  
> By: Conner Graham

## Table of Contents
- TicTacToe Class Protocol - [Protocol Document](https://docs.google.com/document/d/1sAJ5aCkF4F-3unADvujkyA34YAVS8z_h0VBOXrvOQ34/edit?usp=sharing)
- [Environment Constants](#environment-constants)
- [Environment Structures](#environment-structures)
- [High-Level Architecture](#high-level-architecture)
- [Low-Level Architecturet](#low-level-architecture)

## Environment Constants
```C#
VERSION = 5             // protocol version number

NUM_ARGS = 2            // number of command line arguments
BACKLOG_MAX = TBD       // max length for queue of pending connections
ROWS = 3                // number of rows for the TicIacToe board
COLUMNS = 3             // number of columns for the TicIacToe board
CMD_SIZE = 5            // game command size (in bytes)
MAX_GAMES = TBD         // maximum number of games that can be played simultaneously
P1_MARK = TBD           // baord marker used for Player 1
P2_MARK = TBD           // baord marker used for Player 2

// COMMANDS
NEW_GAME = 0x00         // command to begin a new game
MOVE = 0x01             // command to issue a move
GAME_OVER = 0x02        // command to end a game
```

## Environment Structures
Structure for each TicTacToe game.
```C
struct TTT_Game {
    int sd;                         // socket descriptor for connected player
    int gameNum;                    // game number
    int seqNum;                     // sequence number game currently on
    int winner;                     // player who won, 0 if draw, -1 if game ongoing
    char board[ROWS*COLUMNS];       // TicTacToe game board state
};
```
Structure to send and recieve player messages.
```C
struct Buffer {
    char version;   // version number
    char seqNum;    // sequence number
    char command;   // player command
    char data;      // data for command if applicable
    char gameNum;   // game number
};
```

## High-Level Architecture
At a high level, the server application attempts to validate and extract the arguments passed
to the application. It then attempts to create and bind the server endpoint. If everything was
successful, the TicTacToe server is started. If an error occurs before the server is started,
the program terminates and prints appropriate error messages, otherwise an error message is
printed and the server continues.
```C
int main(int argc, char *argv[]) {
    /* check that the arg count is correct */
    if (!correct) exit(EXIT_FAILURE);
    extract_args(params...);
    create_endpoint(params...);
    /* listen for waiting clients */
    if (!error) tictactoe(params...);
    return 0;
}
```

## Low-Level Architecture
Extracts the user provided arguments to their respective local variables and performs
validation on their formatting. If any errors are found, the function terminates the process.
```C
void extractArgs(params...) {
    /* extract and validate remote port number */
    if (!valid) exit(EXIT_FAILURE);
}
```
Creates the comminication endpoint with the provided IP address and port number. If any
errors are found, the function terminates the process.
```C
int create_endpoint(params...) {
    /* attempt to create socket */
    if (created) {
        /* initialize socket with params from user */
    } else {
        exit(EXIT_FAILURE);
    }
    /* attempt to bind socket to address */
    if (!bind) {
        exit(EXIT_FAILURE);
    }
    return socket-descriptor;
}
```
Initializes a set of game boards and processes any commands received from other players. These
commands can include initializing a game of TicTacToe when a player requests one, responding
to other players moves until a winner is found or the game is a draw, or ending a game of
TicTacToe. If a player leaves the game (i.e. closes the connection) the game is reset for
another player to play.
```C
void tictactoe(params...) {
    /* initialize all games */
    while (TRUE) {
        /* set server and games with connected clients as active */
        /* wait to receive new connection or command from active games */
        if (new connection) {
            /* accept player connection */
            /* find open game to assign connection to */
            if (open game) {
                /* assign connection to game */
            } else {
                /* close client conection */
            }
        }
        if (commands waiting to be received) {
            /* load game to receive command */
            get_command(params...);
            validate_sequence_num(params...);
            if (valid) {
                /* process command for game */
            } else {
                reset_game(params...);
            }            
        }
    }
}
```
- Gets a command from the remote player and attempts to validate the data and syntax based on
  the current protocol.
    ```C
    int get_command(params...) {
        /* receive command from remote player */
        if (error) return ERROR_CODE;
        /* check version number */
        if (!valid) return ERROR_CODE;
        /* check sequence number */
        if (!valid) return ERROR_CODE;
        /* check command */
        if (!valid) return ERROR_CODE;
        /* check game number */
        if (!valid) return ERROR_CODE;
        return (number of bytes received for command);
    }
    ```
    - Handles the NEW_GAME command from the remote player. Initializes a new game, if available,
      and sends the first move to the remote player.
        ```C
        void new_game(params...) {
            if (there is an open game) {
                /* initialize the game board */
                /* update game sequence number */
                send_p1_move(params...);
                if (error) {
                    reset_game(params...);
                    return;
                }
                /* update and print game board */
            }
        }
        ```
    - Handles the MOVE command from the remote player. Receives and processes a move from the
      remote player and sends a move back. If the game has ended from a move, an appropriate
      message is printed and the game is reset for a new player.
        ```C
        void move(params...) {
            /* get move from remote player */
            /* check that move is valid */
            if (valid) {
                /* update game sequence number */
                /* update board with Player 2's move */
                if (game over) {
                    send_game_over(params...);
                    return;
                }
                send_p1_move(params...);
                if (error) {
                    reset_game(params...);
                    return;
                }
                /* update board with Player 1's move */
                if (game not over) print board after move exchange */
            } else {
                reset_game(params...);
            }
        }
        ```
    - Handles the GAME_OVER command from the remote player. Resets the current game for new
      players to begin playing.
        ```C
        void game_over(params...) {
            /* print game end message */
            reset_game(params...);
        }
        ```
- Determines whether or not the given sequence number is valid based on the current state of the game.
    ```C
    int validate_sequence_num(params...) {
        if (/* no game found or player address doesn't match */) return (positive value);
        if (SN > game.SN) return ERROR_CODE;    // invalid sequence number
        if (SN = game.SN-1) return 0;           // duplicate sequence number
        if (SN < game.SN-1) return (code for already processed);
    }
    ```
- Determines whether or not the given move is valid based on the current state of the game.
    ```C
    int validate_move(params...) {
        if (move not numer [1-9]) return FALSE;
        if (move has already been made) return FALSE;
        if (game has already ended) return FALSE;
        return TRUE;
    }
    ```
- Sends Player 1's move to the remote player.
    ```C
    int send_p1_move(params...) {
        /* get move to send to remote player */
        /* pack move info into message */
        /* send move to remote player */
        if (error) return ERROR_CODE;
        return (move);
    }
    ```
- Checks if the current game has ended and prints the appropriate message if so.
    ```C
    int check_game_over(params...) {
        /* check if somebody won */
        if (winner) /* set and print winner info */;
        /* check for draw */
        if (draw) /* set and print draw info */;
        /* check if game still in progress */
        if (!winner && !draw) return FALSE;
        /* print final board state */
        return TRUE;
    }
    ```
- Sends GAME_OVER command to the remote player and resets the game.
    ```C
    void send_game_over(params...) {
        /* pack command info into message */
        /* send command to remote player */
        reset_game(params...);
    }
    ```
- Resets the current game for a new player.
    ```C
    void reset_game(params...) {
        /* check if game connected to client */
        if (connected) /* close connection */;
        /* reset game attributes */
        /* reset game board */
    }
    ```
