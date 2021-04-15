/***********************************************************/
/* This program is a 'net-enabled' version of tictactoe.   */
/* Two users, Player 1 and Player 2, send moves back and   */
/* forth, between two computers.                           */
/***********************************************************/

/* #include files go here */
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

/**************************/
/* ENVIRONMENT STRUCTURES */
/**************************/

/* The number of rows for the TicIacToe board. */
#define ROWS 3
/* The number of columns for the TicIacToe board. */
#define COLUMNS 3
/* The size (in bytes) of the game state. */
#define GAME_SIZE (ROWS * COLUMNS)
/* The maximum number of games the server can play simultaneously. */
#define MAX_GAMES 10

/* Structure to send and recieve UDP player messages. */
struct UDP_Buffer {
    char version;           // version number
    char command;           // player command
    unsigned short port;    // TODO
};

/* Structure to send and recieve TCP player messages. */
struct TCP_Buffer {
    char version;   // version number
    char command;   // player command
    char data;      // data for command if applicable
    char gameNum;   // game number
};

/* Structure for each game of TicTacToe. */
struct TTT_Game {
    int sd;                         // socket descriptor for connected player
    int gameNum;                    // game number
    int winner;                     // player who won, 0 if draw, -1 if game not over
    char board[GAME_SIZE];          // TicTacToe game board state
};

/* Structure for TODO . */
struct Server {
    int sd;                                 // socket descriptor for the server
    int mcd;                                // socket descriptor for the multicast group
    int maxSD;                              // TODO
    fd_set socketFDS;                       // TODO
    struct sockaddr_in serverAddr;          // TODO
    struct sockaddr_in mulicastAddr;        // TODO
    struct TTT_Game gameRoster[MAX_GAMES];  // TODO
};

/*****************************/
/* GENERAL PURPOSE FUNCTIONS */
/*****************************/

/* The number of command line arguments. */
#define NUM_ARGS 2
/* The maximum size of a buffer for the program. */
#define BUFFER_SIZE 100
/* The error code used to signal an invalid move. */
#define ERROR_CODE -1

void print_error(const char *msg, int errnum, int terminate);
void handle_init_error(const char *msg, int errnum);
void extract_args(char *argv[], int *port);

/********************************/
/* SOCKET AND NETWORK FUNCTIONS */
/********************************/

/* The protocol version number used. */
#define VERSION 6
/* The maximum length to which the queue of pending connections may grow. */
#define BACKLOG_MAX 5
/* TODO */
#define MC_PORT 1818
/* TODO */
#define MC_GROUP "239.0.0.1"

int create_endpoint(struct sockaddr_in *socketAddr, int type, unsigned long address, int port);
void add_multicast_group(const struct Server *serv, const char *groupAddr);
void print_server_info(const struct Server *serv);

/******************************/
/* TIC-TAC-TOE GAME FUNCTIONS */
/******************************/

/* The baord marker used for Player 1 */
#define P1_MARK 'X'
/* The baord marker used for Player 2 */
#define P2_MARK 'O'

void init_shared_state(struct TTT_Game *game);
int load_shared_state(struct TTT_Game *game);
void init_game_roster(struct Server *serv);
int find_open_game(const struct Server *serv);
int get_udp_command(int sd, struct sockaddr_in *playerAddr, struct UDP_Buffer *datagram);
void send_game_available(const struct Server *serv, const struct sockaddr_in *playerAddr);
int get_tcp_command(int sd, struct TCP_Buffer *msg);
void send_game_over(struct TTT_Game *game);
int minimax(struct TTT_Game *game, int depth, int isMax);
int find_best_move(struct TTT_Game *game);
int validate_move(int choice, const struct TTT_Game *game);
int send_p1_move(struct TTT_Game *game);
int check_win(const struct TTT_Game *game);
int check_draw(const struct TTT_Game *game);
int check_game_over(struct TTT_Game *game);
void print_board(const struct TTT_Game *game);
void reset_game(struct TTT_Game *game);
void tictactoe(struct Server *serv);

/*******************/
/* PLAYER COMMANDS */
/*******************/

/* The size (in bytes) of each UDP game command. */
#define UDP_CMD_SIZE (sizeof(struct UDP_Buffer))
/* The size (in bytes) of each TCP game command. */
#define TCP_CMD_SIZE (sizeof(struct TCP_Buffer))

/* Function pointer type for function to handle player commands. */
typedef void (*Command_Handler)(const struct TCP_Buffer *msg, struct TTT_Game *game);
/* The TCP command to begin a new game */
#define NEW_GAME 0x00
/* The TCP command to issue a move. */
#define MOVE 0x01
/* The TCP command to signal that the game has ended. */
#define GAME_OVER 0x02
/* The TCP command to resume a previously started game. */
#define RESUME_GAME 0x03

/* The UDP command for a client to request an open game from the multicast group. */
#define REQUEST_GAME 0x04
/* The UDP command from a server in the mulicast group that a game is available. */
#define GAME_AVAILABLE 0x05

void new_game(const struct TCP_Buffer *msg, struct TTT_Game *game);
void move(const struct TCP_Buffer *msg, struct TTT_Game *game);
void game_over(const struct TCP_Buffer *msg, struct TTT_Game *game);
void resume_game(const struct TCP_Buffer *msg, struct TTT_Game *game);
void request_game(const struct Server *serv, const struct sockaddr_in *playerAddr);

/**
 * @brief This program creates and sets up a TicTacToe server which acts as Player 1 in a
 * 2-player game of TicTacToe. This server creates a server socket for the clients to communicate
 * with, listens for remote client UDP DAGAGRAM packets, and then initiates a simple
 * game of TicTacToe in which Player 1 and Player 2 take turns making moves which they send to
 * the other player. If an error occurs before the "New Game" command is received, the program
 * terminates and prints appropriate error messages, otherwise an error message is printed and
 * the program searches for a new player waiting to play.
 * 
 * @param argc Non-negative value representing the number of arguments passed to the program
 * from the environment in which the program is run.
 * @param argv Pointer to the first element of an array of argc + 1 pointers, of which the
 * last one is NULL and the previous ones, if any, point to strings that represent the
 * arguments passed to the program from the host environment. If argv[0] is not a NULL
 * pointer (or, equivalently, if argc > 0), it points to a string that represents the program
 * name, which is empty if the program name is not available from the host environment.
 * @return If the return statement is used, the return value is used as the argument to the
 * implicit call to exit(). The values zero and EXIT_SUCCESS indicate successful termination,
 * the value EXIT_FAILURE indicates unsuccessful termination.
 */
int main(int argc, char *argv[]) {
    int portNumber;
    struct Server serv;

    /* If arg count correct, extract arguments to their respective variables */
    if (argc != NUM_ARGS) handle_init_error("argc: Invalid number of command line arguments", 0);
    extract_args(argv, &portNumber);

    /* Create multicast socket and join multicast group */
    serv.mcd = create_endpoint(&serv.mulicastAddr, SOCK_DGRAM, INADDR_ANY, MC_PORT);
    add_multicast_group(&serv, MC_GROUP);
    /* Create server socket */
    serv.sd = create_endpoint(&serv.serverAddr, SOCK_STREAM, INADDR_ANY, portNumber);
    /* Print server information and listen for waiting clients */
    if (listen(serv.sd, BACKLOG_MAX) == 0) {
        print_server_info(&serv);
        /* Start the TicTacToe server */
        tictactoe(&serv);
    } else {
        print_error("listen", errno, 0);
        if (close(serv.mcd) < 0) print_error("main: close-multicast", errno, 0);
        if (close(serv.sd) < 0) print_error("main: close-server", errno, 0);
    }

    return 0;
}

/**
 * @brief Prints the provided error message and corresponding errno message (if present) and
 * terminates the process if asked to do so.
 * 
 * @param msg The error description message to display.
 * @param errnum This is the error number, usually errno.
 * @param terminate Whether or not the process should be terminated.
 */
void print_error(const char *msg, int errnum, int terminate) {
    /* Check for valid error code and generate error message */
    if (errnum) {
        printf("ERROR: %s: %s\n", msg, strerror(errnum));
    } else {
        printf("ERROR: %s\n", msg);
    }
    /* Exits process if it should be terminated */
    if (terminate) exit(EXIT_FAILURE);
}

/**
 * @brief Prints a string describing the initialization error and provided error number (if
 * nonzero), the correct command usage, and exits the process signaling unsuccessful termination. 
 * 
 * @param msg The error description message to display.
 * @param errnum This is the error number, usually errno.
 */
void handle_init_error(const char *msg, int errnum) {
    print_error(msg, errnum, 0);
    printf("Usage is: tictactoeServer <remote-port>\n");
    /* Exits the process signaling unsuccessful termination */
    exit(EXIT_FAILURE);
}

/**
 * @brief Extracts the user provided arguments to their respective local variables and performs
 * validation on their formatting. If any errors are found, the function terminates the process.
 * 
 * @param argv Pointer to the first element of an array of argc + 1 pointers, of which the
 * last one is NULL and the previous ones, if any, point to strings that represent the
 * arguments passed to the program from the host environment. If argv[0] is not a NULL
 * pointer (or, equivalently, if argc > 0), it points to a string that represents the program
 * name, which is empty if the program name is not available from the host environment.
 * @param port The remote port number that the server should listen on
 */
void extract_args(char *argv[], int *port) {
    /* Extract and validate remote port number */
    *port = strtol(argv[1], NULL, 10);
    if (*port < 1 || *port != (u_int16_t)(*port)) handle_init_error("extract_args: Invalid port number", 0);
}

/**
 * @brief Creates the comminication endpoint with the provided IP address and port number. If any
 * errors are found, the function terminates the process.
 * 
 * @param socketAddr The socket address structure created for the comminication endpoint.
 * @param address The IP address for the socket address structure.
 * @param port The port number for the socket address structure.
 * @return The socket descriptor of the created comminication endpoint.
 */
int create_endpoint(struct sockaddr_in *socketAddr, int type, unsigned long address, int port) {
    int sd;
    /* Create socket */
    if ((sd = socket(AF_INET, type, 0)) >= 0) {
        /* TODO */
        bzero((char *)socketAddr, sizeof(struct sockaddr_in *));
        /* TODO */
        socketAddr->sin_family = AF_INET;
        /* Assign IP address to socket */
        socketAddr->sin_addr.s_addr = htonl(address);
        /* Assign port number to socket */
        socketAddr->sin_port = htons(port);
    } else {
        print_error("create_endpoint: socket", errno, 1);
    }
    /* Bind socket to communication endpoint */
    if (bind(sd, (struct sockaddr *)socketAddr, sizeof(struct sockaddr_in)) < 0) {
        print_error("create_endpoint: bind", errno, 1);
    }
    if (type == SOCK_DGRAM) {
        printf("[+]DGRAM socket created successfully.\n");
    } else if (type == SOCK_STREAM) {
        printf("[+]STREAM socket created successfully.\n");
    } else {
        printf("[+]UNKNOWN socket created successfully.\n");
    }
    return sd;
}

/**
 * @brief TODO
 * 
 * @param serv 
 * @param groupAddr 
 */
void add_multicast_group(const struct Server *serv, const char *groupAddr) {
    struct ip_mreq mreq;
    /* TODO */
    mreq.imr_multiaddr.s_addr = inet_addr(groupAddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    /* TODO */
    if (setsockopt(serv->mcd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(struct ip_mreq)) < 0) {
        print_error("add_multicast_group: setsockopt", errno, 1);
    }
    printf("Server joined multicast group at %s (port %hu)\n", groupAddr, serv->mulicastAddr.sin_port);
}

/**
 * @brief Prints the server information needed for the client to comminicate with the server.
 * 
 * @param serv TODO
 */
void print_server_info(const struct Server *serv) {
    int hostname;
    char hostbuffer[BUFFER_SIZE], *IP_addr;
    struct hostent *host_entry;

    /* Retrieve the hostname */
    if ((hostname = gethostname(hostbuffer, sizeof(hostbuffer))) == -1) {
        print_error("print_server_info: gethostname", errno, 1);
    }
    /* Retrieve the host information */
    if ((host_entry = gethostbyname(hostbuffer)) == NULL) {
        print_error("print_server_info: gethostbyname", errno, 1);
    }
    /* Convert the host internet network address to an ASCII string */
    IP_addr = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));
    /* Print the IP address and port number for the server */
    printf("Server listening at %s on port %hu\n", IP_addr, serv->serverAddr.sin_port);
}

/**
 * @brief Initializes the starting state of the game board that both players start with.
 * 
 * @param game The current game of TicTacToe being played.
 */
void init_shared_state(struct TTT_Game *game) {    
    int i;
    /* Initializes the shared state (aka the board)  */
    for (i = 1; i <= sizeof(game->board); i++) {
        game->board[i-1] = i + '0';
    }
}

/**
 * @brief TODO
 * 
 * @param game The current game of TicTacToe being played.
 * @return 
 */
int load_shared_state(struct TTT_Game *game) {    
    int i, bytes = 0, moveCountP1 = 0, moveCountP2 = 0;
    char boardState[GAME_SIZE];
    /* Receive shared state (aka the board) from remote player */
    while (bytes < GAME_SIZE) {
        int rv;
        /* Receive partial board state */
        if ((rv = recv(game->sd, boardState+bytes, GAME_SIZE, 0)) <= 0) {
            if (rv == 0) {
                print_error("load_shared_state: Player 2 has disconnected", 0, 0);
            } else {
                print_error("load_shared_state", errno, 0);
            }
            return 0;
        }
        /* Update total bytes received for shared state */
        bytes += rv;
    }

    /* TODO */
    for (i = 0; i < GAME_SIZE; i++) {
        char mark = boardState[i];
        if (mark == 0 || mark == P1_MARK || mark == P2_MARK) {
            if (mark != 0) {
                game->board[i] = mark;
                (mark == P1_MARK) ? moveCountP1++ : moveCountP2++;
            }
        } else {
            print_error("load_shared_state: The received board contains invalid marks", 0, 0);
            return 0;
        }
    }
    /* Validate board TODO */
    if (moveCountP1 != moveCountP2) {
        print_error("load_shared_state: Board state contains an invalid number of moves", 0, 0);
        return 0;
    }

    return 1;
}

/**
 * @brief Resets the current game for a new player.
 * 
 * @param game The current game of TicTacToe being played.
 */
void reset_game(struct TTT_Game *game) {
    /* Check if game has been initialized */
    if (game->gameNum != 0) {
        printf("Game #%d has ended. Resetting game for new player\n", game->gameNum);
        /* Close client connection to game */
        if (close(game->sd) < 0) print_error("reset_game: close-connection", errno, 0);
    }
    /* Reset game attributes */
    game->sd = -1;
    game->winner = -1;
    /* Reset game board */
    init_shared_state(game);
}

/**
 * @brief Initializes the starting state of each game of TicTacToe in the current game roster.
 * 
 * @param serv TODO
 */
void init_game_roster(struct Server *serv) {
    int i;
    printf("[+]Initializing shared game states.\n");
    memset(serv->gameRoster, 0, sizeof(serv->gameRoster));
    /* Iterates over all games */
    for (i = 0;  i < MAX_GAMES; i++) {
        struct TTT_Game *game = &serv->gameRoster[i];
        /* Initialize current game attributes to default values */
        reset_game(game);
        /* Set current game number */
        game->gameNum = i+1;
    }
}

/**
 * @brief Finds an open game of TicTacToe to play if one is available.
 * 
 * @param serv TODO
 * @return The index of an open game if one is available, otherwise an error code is returned.
 */
int find_open_game(const struct Server *serv) {
    int i, gameIndex = ERROR_CODE;
    /* Searches over all games */
    for (i = 0; i < MAX_GAMES; i++) {
        const struct TTT_Game *game = &serv->gameRoster[i];
        /* Check that a client is not connected to the game */
        if (game->sd < 0) {
            gameIndex = i;
            break;
        }
    }
    return gameIndex;
}

/**
 * @brief Gets a UDP command from the remote player and attempts to validate the data and
 * syntax based on the current protocol.
 * 
 * @param sd The socket descriptor of the multicast group.
 * @param playerAddr The address of the remote player.
 * @param datagram The datagram to store the command that the remote player sent.
 * @return The number of bytes received for the command, or an error code if an error occured. 
 */
int get_udp_command(int sd, struct sockaddr_in *playerAddr, struct UDP_Buffer *datagram) {
    int bytes = 0;
    socklen_t fromLength = sizeof(struct sockaddr_in);
    /* Receive and validate command from remote player */
    if ((bytes = recvfrom(sd, datagram, UDP_CMD_SIZE, 0, (struct sockaddr *)playerAddr, &fromLength)) <= 0) {
        /* Check for error receiving command */
        if (bytes == 0) {
            print_error("get_udp_command: Received empty datagram. Datagram discarded", 0, 0);
        } else {
            print_error("get_udp_command", errno, 0);
        }
        return ERROR_CODE;
    }
    /* Validate the received message */
    if (datagram->version != VERSION) {  // check for correct version
        print_error("get_udp_command: Protocol version not supported", 0, 0);
        return ERROR_CODE;
    } else if (datagram->command < REQUEST_GAME || datagram->command > GAME_AVAILABLE) {  // check for valid command
        print_error("get_udp_command: Invalid UDP command", 0, 0);
        return ERROR_CODE;
    }
    return bytes;
}

/**
 * @brief TODO
 * 
 * @param serv TODO
 * @param playerAddr The address of the remote player.
 */
void request_game(const struct Server *serv, const struct sockaddr_in *playerAddr) {
    printf("A remote player issued a REQUEST_GAME command\n");
    /* TODO */
    if (find_open_game(serv) >= 0) {
        send_game_available(serv, playerAddr);
    } else {
        print_error("request_game: Unable to find an open game", 0, 0);
    }
}

/**
 * @brief TODO
 * 
 * @param serv TODO
 * @param playerAddr The address of the remote player.
 */
void send_game_available(const struct Server *serv, const struct sockaddr_in *playerAddr) {
    struct UDP_Buffer datagram = {0};
    /* Pack command information into datagram */
    datagram.version = VERSION;
    datagram.command = GAME_AVAILABLE;
    datagram.port = serv->serverAddr.sin_port;
    /* Send the command to the remote player */
    printf("Server sent the GAME_AVAILABLE command to the remote player\n");
    if (sendto(serv->mcd, &datagram, UDP_CMD_SIZE, 0, (struct sockaddr *)playerAddr, sizeof(struct sockaddr_in)) < 0) {
        print_error("send_game_available", errno, 0);
    }
}

/**
 * @brief Gets a TCP command from the remote player and attempts to validate the data and
 * syntax based on the current protocol.
 * 
 * @param sd The socket descriptor of the connected player's comminication endpoint.
 * @param msg The buffer to store the command that the remote player sent.
 * @return The number of bytes received for the command, or an error code if an error occured. 
 */
int get_tcp_command(int sd, struct TCP_Buffer *msg) {
    int bytes = 0;
    /* Receive message from remote player */
    while (bytes < TCP_CMD_SIZE) {
        int rv;
        /* Receive partial message */
        if ((rv = recv(sd, msg+bytes, TCP_CMD_SIZE, 0)) <= 0) {
            if (rv == 0) {
                print_error("get_tcp_command: Player 2 has disconnected", 0, 0);
            } else {
                print_error("get_tcp_command", errno, 0);
            }
            return ERROR_CODE;
        }
        /* Update total bytes received for message */
        bytes += rv;
    }
    /* Validate the received message */
    if (msg->version != VERSION) {  // check for correct version
        print_error("get_tcp_command: Protocol version not supported", 0, 0);
        return ERROR_CODE;
    } else if (msg->command < NEW_GAME || msg->command > RESUME_GAME) {  // check for valid command
        print_error("get_tcp_command: Invalid TCP command", 0, 0);
        return ERROR_CODE;
    } else if (msg->command != NEW_GAME && (msg->gameNum < 1 || msg->gameNum > MAX_GAMES)) { // check for valid game number
        print_error("get_tcp_command: Invalid game number", 0, 0);
        return ERROR_CODE;
    }
    return bytes;
}

/**
 * @brief Handles the NEW_GAME command from the remote player. Initializes a new game, if
 * available, and sends the first move to the remote player.
 * 
 * @param msg The message containing the command that the remote player sent.
 * @param game The current game of TicTacToe being played.
 */
void new_game(const struct TCP_Buffer *msg, struct TTT_Game *game) {
    int move;
    printf("The remote player issued a NEW_GAME command\n");
    /* Initialize the board */
    init_shared_state(game);
    /* Get first move to send to remote player */
    if ((move = send_p1_move(game)) == ERROR_CODE) {
        /* Reset game if there was an error sending the move */
        reset_game(game);
        return;
    }
    /* Update and print game board */
    game->board[move-1] = P1_MARK;
    print_board(game);
}

/**
 * @brief Handles the MOVE command from the remote player. Receives and processes a move
 * from the remote player and sends a move back. If the game has ended from a move, an
 * appropriate message is printed. If the game ends from a move from the remote player,
 * a GAME_OVER command is rent back in response.
 * 
 * @param msg The message containing the command that the remote player sent.
 * @param game The current game of TicTacToe being played.
 */
void move(const struct TCP_Buffer *msg, struct TTT_Game *game) {
    /* Get move from remote player */
    int move = msg->data - '0';
    printf("The remote player issued a MOVE command\n");
    printf("Player 2 chose the move:  %c\n", msg->data);
    /* Check that the received move is valid */
    if (validate_move(move, game)) {
        /* Update the board (for Player 2) and check if someone won */
        game->board[move-1] = P2_MARK;
        if (check_game_over(game)) {
            /* If Player 2 won, send GAME_OVER command and reset game */
            send_game_over(game);
            return;
        }
        /* If nobody won, make a move to send to the remote player */
        if ((move = send_p1_move(game)) == ERROR_CODE) {
            /* Reset game if there was an error sending the move */
            reset_game(game);    
            return;
        }
        /* Update the board (for Player 1) and check if someone won after the exchange */
        game->board[move-1] = P1_MARK;
        if (!check_game_over(game)) print_board(game);
    } else {
        reset_game(game);
    }
}

/**
 * @brief Handles the GAME_OVER command from the remote player. Determines the reason for
 * ending the game, prints the appropriate message, and resets the game.
 * 
 * @param msg The message containing the command that the remote player sent.
 * @param game The current game of TicTacToe being played.
 */
void game_over(const struct TCP_Buffer *msg, struct TTT_Game *game) {
    printf("The remote player issued a GAME_OVER command\n");
    printf("Player 2 has signaled that the game is over\n");
    /* Check if the game is actually over */
    if (game->winner < 0) {
        /* If not over, player decided to leave prematurely */
        print_error("game_over: Game is still in progress", 0, 0);
        printf("Player 2 has decided to leave the game\n");
    } else {
        /* If over, print appropriate message for who won */
        (game->winner == 0) ? printf("==>\a It's a draw\n") : printf("==>\a Player %d wins\n", game->winner);
    }
    /* Reset the game */
    reset_game(game);
}

/**
 * @brief TODO
 * 
 * @param msg The message containing the command that the remote player sent.
 * @param game The current game of TicTacToe being played.
 */
void resume_game(const struct TCP_Buffer *msg, struct TTT_Game *game) {
    int move;
    printf("The remote player issued a RESUME_GAME command\n");
    /* TODO */
    if (load_shared_state(game)) {
        print_board(game);
    } else {
        reset_game(game);
        return;
    }
    /* TODO */
    if (check_game_over(game)) {
        /* If Player 2 won, send GAME_OVER command and reset game */
        send_game_over(game);
        return;
    }
    /* If nobody won, make a move to send to the remote player */
    if ((move = send_p1_move(game)) == ERROR_CODE) {
        /* Reset game if there was an error sending the move */
        reset_game(game);
        return;
    }
    /* Update and print game board */
    game->board[move-1] = P1_MARK;
    if (!check_game_over(game)) print_board(game);
}

/**
 * @brief Determines whether a given move is legal (i.e. number 1-9) and valid (i.e. hasn't
 * already been played) for the current game.
 * 
 * @param choice The player move to be validated.
 * @param game The current game of TicTacToe being played.
 * @return True if the given move if valid based on the current game, false otherwise. 
 */
int validate_move(int choice, const struct TTT_Game *game) {
    /* Check to see if the choice is a move on the board */
    if (choice < 1 || choice > 9) {
        print_error("Invalid move: Must be a number [1-9]", 0, 0);
        return 0;
    }
    /* Check to see if the square chosen has a digit in it, if */
    /* square 8 has an '8' then it is a valid choice */
    if (game->board[choice-1] != (choice + '0')) {
        print_error("Invalid move: Square already taken", 0, 0);
        return 0;
    }
    /* Check to see if the game has already ended */
    if (game->winner > 0) {
        print_error("Invalid move: Winning move has already been made", 0, 0);
        return 0;
    }
    return 1;
}

/**
 * @brief Provides an optimal move for the maximizing player assuming that minimizing player
 * is also playing optimally.
 * 
 * @param game The current game of TicTacToe being played.
 * @param depth The current depth in game tree.
 * @param isMax Whether it is the maximizers turn or not.
 * @return The best score achievable for the maximizer based on the current state of the game.
 */
int minimax(struct TTT_Game *game, int depth, int isMax) {
    /* Get score for current turn */
    int score = check_win(game);
    /* Check for base case */
    if (score > 0) {    // maximizer won
        return score - depth;
    } else if (score < 0) {    // minimizer won
        return score + depth;
    } else if (check_draw(game)) {  // nobody won
        return 0;
    } else {
        /* Initialize best score for maximizer/minimizer */
        int i, best = (isMax) ? INT32_MIN : INT16_MAX;
        if (isMax) {    // maximizers turn
            /* Searches over all possible moves */
            for (i = 0; i < sizeof(game->board); i++) {
                /* Checks that current move is valid based on the current board */
                if (game->board[i] == (i+1)+'0') {
                    int value;
                    /* Make the move */
                    game->board[i] = P1_MARK;
                    /* Get best score for move and update best move if the score was better */
                    if ((value = minimax(game, depth+1, !isMax)) > best) best = value;
                    /* Undo previous move */
                    game->board[i] = (i+1)+'0';
                }
            }
            return best;
        } else {    // minimizers turn
            /* Searches over all possible moves */
            for (i = 0; i < sizeof(game->board); i++) {
                /* Checks that current move is valid based on the current board */
                if (game->board[i] == (i+1)+'0') {
                    int value;
                    /* Make the move */
                    game->board[i] = P2_MARK;
                    /* Get best score for move and update best move if the score was better */
                    if ((value = minimax(game, depth+1, !isMax)) < best) best = value;
                    /* Undo previous move */
                    game->board[i] = (i+1)+'0';
                }
            }
            return best;
        }
    }
}

/**
 * @brief Finds the optimal move to make to win the game based on the current state of
 * the game board.
 * 
 * @param game The current game of TicTacToe being played.
 * @return The optimal move to make in order to win. 
 */
int find_best_move(struct TTT_Game *game) {
    int i, bestMove = -1, bestValue = INT32_MIN;
    /* Searches over all possible moves */
    for (i = 0; i < sizeof(game->board); i++) {
        /* Checks that current move is valid based on the current board */
        if (game->board[i] == (i+1)+'0') {
            int moveValue;
            /* Make the move */
            game->board[i] = P1_MARK;
            /* Get the move score */
            moveValue = minimax(game, 0, 0);
            /* Undo previous move */
            game->board[i] = (i+1)+'0';
            /* Update the best move if the current score was better */
            if (moveValue > bestValue) {
                bestValue = moveValue;
                bestMove = i+1;
            }
        }
    }
    return bestMove;
}

/**
 * @brief Sends Player 1's move to the remote player.
 * 
 * @param game The current game of TicTacToe being played.
 * @return The move that was sent, or an error code if there was an issue. 
 */
int send_p1_move(struct TTT_Game *game) {
    struct TCP_Buffer msg = {0};
    /* Get move to send to remote player */
    int move = find_best_move(game);
    while (!validate_move(move, game)) move = find_best_move(game);
    /* Pack move information into message */
    msg.version = VERSION;
    msg.command = MOVE;
    msg.data = move + '0';
    msg.gameNum = game->gameNum;
    /* Send the move to the remote player */
    printf("Server sent the move:  %c\n", msg.data);
    if (send(game->sd, &msg, TCP_CMD_SIZE, MSG_NOSIGNAL) < 0) {
        print_error("send_p1_move", errno, 0);
        return ERROR_CODE;
    }
    return (msg.data - '0');
}

/**
 * @brief Determines if someone has won the game yet or not.
 * 
 * @param game The current game of TicTacToe being played.
 * @return True if a player has won the game and false if the game is still going on. 
 */
int check_win(const struct TTT_Game *game) {
    const int score = sizeof(game->board) + 1;
    /***********************************************************************/
    /* Brute force check to see if someone won. Return a +/- score if the  */
    /* game is 'over' or return 0 if game should go on.                    */
    /***********************************************************************/
    if (game->board[0] == game->board[1] && game->board[1] == game->board[2]) { // row matches
        return (game->board[0] == P1_MARK) ? score : -score;
    } else if (game->board[3] == game->board[4] && game->board[4] == game->board[5]) { // row matches
        return (game->board[3] == P1_MARK) ? score : -score;
    } else if (game->board[6] == game->board[7] && game->board[7] == game->board[8]) { // row matches
        return (game->board[6] == P1_MARK) ? score : -score;
    } else if (game->board[0] == game->board[3] && game->board[3] == game->board[6]) { // column matches
        return (game->board[0] == P1_MARK) ? score : -score;
    } else if (game->board[1] == game->board[4] && game->board[4] == game->board[7]) { // column matches
        return (game->board[1] == P1_MARK) ? score : -score;
    } else if (game->board[2] == game->board[5] && game->board[5] == game->board[8]) { // column matches
        return (game->board[2] == P1_MARK) ? score : -score;
    } else if (game->board[0] == game->board[4] && game->board[4] == game->board[8]) { // diagonal matches
        return (game->board[0] == P1_MARK) ? score : -score;
    } else if (game->board[2] == game->board[4] && game->board[4] == game->board[6]) { // diagonal matches
        return (game->board[2] == P1_MARK) ? score : -score;
    } else {
        return 0;  // return of 0 means keep playing
    }
}

/**
 * @brief Determines if there are moves left in the game to be made or not.
 * 
 * @param game The current game of TicTacToe being played.
 * @return True if there are no moves left to be made, false otherwise. 
 */
int check_draw(const struct TTT_Game *game) {
    int i;
    /* Check each board square */
    for (i = 0; i < sizeof(game->board); i++) {
        /* Check if current square has been played */
        if (game->board[i] == (i+1)+'0') return 0;
    }
    return 1;
}

/**
 * @brief Prints out the current state of the game board nicely formatted.
 * 
 * @param game The current game of TicTacToe being played.
 */
void print_board(const struct TTT_Game *game) {
    /*****************************************************************/
    /* Brute force print out the board and all the squares/values    */
    /*****************************************************************/
    /* Print header info */
    printf("\n\n\tTicTacToe Game #%d\n\n", game->gameNum);
    printf("Player 1 (%c)  -  Player 2 (%c)\n\n\n", P1_MARK, P2_MARK);
    /* Print current state of board */
    printf("     |     |     \n");
    printf("  %c  |  %c  |  %c \n", game->board[0], game->board[1], game->board[2]);
    printf("_____|_____|_____\n");
    printf("     |     |     \n");
    printf("  %c  |  %c  |  %c \n", game->board[3], game->board[4], game->board[5]);
    printf("_____|_____|_____\n");
    printf("     |     |     \n");
    printf("  %c  |  %c  |  %c \n", game->board[6], game->board[7], game->board[8]);
    printf("     |     |     \n\n");
}

/**
 * @brief Checks if the current game has ended and prints the appropriate message.
 * 
 * @param game The current game of TicTacToe being played.
 * @return True if the game has ended, false otherwise. 
 */
int check_game_over(struct TTT_Game *game) {
    int score;
    /* Check if somebody won the game */
    if ((score = check_win(game))) {
        game->winner = (score > 0) ? 1 : 2;
    } else if (check_draw(game)) {
        game->winner = 0;
    } else {
        return 0;
    }
    /* Print final game board and winning player */
    print_board(game);
    (game->winner == 0) ? printf("==>\a It's a draw\n") : printf("==>\a Player %d wins\n", game->winner);
    return 1;
}

/**
 * @brief Sends GAME_OVER command to the remote player and resets the current game for
 * a new player.
 * 
 * @param game The current game of TicTacToe being played.
 */
void send_game_over(struct TTT_Game *game) {
    struct TCP_Buffer msg = {0};
    /* Pack command information into message */
    msg.version = VERSION;
    msg.command = GAME_OVER;
    msg.gameNum = game->gameNum;
    /* Send the command to the remote player */
    printf("Server sent the GAME_OVER command to Player 2\n");
    if (send(game->sd, &msg, TCP_CMD_SIZE, MSG_NOSIGNAL) < 0) {
        print_error("send_game_over", errno, 0);
    }
    /* Reset the game */
    reset_game(game);
}

/**
 * @brief Plays multiple games of TicTacToe with remoye players that end when either
 * someone wins, there is a draw, or the remote player leaves the game.
 * 
 * @param serv TODO
 */
void tictactoe(struct Server *serv) {
    serv->maxSD = (serv->mcd > serv->sd) ? serv->mcd : serv->sd;
    Command_Handler commands[] = {new_game, move, game_over, resume_game};

    /* Initialize all games */
    init_game_roster(serv);
    /* Play all the games */
    while (1) {
        int i;
        /* Clear previously processed games */
        FD_ZERO(&serv->socketFDS);
        /* Set the mulicast group and the server to be active */
        FD_SET(serv->mcd, &serv->socketFDS);
        FD_SET(serv->sd, &serv->socketFDS);
        /* Set games that have a client connection to be active */
        for (i = 0; i < MAX_GAMES; i++) {
            int gameSD = serv->gameRoster[i].sd;
            if (gameSD > 0) {
                FD_SET(gameSD, &serv->socketFDS);
                if (gameSD > serv->maxSD) serv->maxSD = gameSD;
            }
        }

        /* Block until there is a new connection or a command is received */
        printf("[+]Waiting for other players to issue commands...\n");
        if (select(serv->maxSD+1, &serv->socketFDS, NULL, NULL, NULL) == -1) {
            print_error("select", errno, 0);
            continue;
        }

        /* Check if a command was received from the multicast group */
        if (FD_ISSET(serv->mcd, &serv->socketFDS)) {
            struct sockaddr_in clientAddress;
            struct UDP_Buffer datagram = {0};
            if (get_udp_command(serv->mcd, &clientAddress, &datagram) > 0) {
                printf("********  Multicast Group  ********\n");
                /* Process received command */
                switch (datagram.command) {
                    case REQUEST_GAME:
                        request_game(serv, &clientAddress);
                        break;
                    case GAME_AVAILABLE:
                        print_error("tictactoe: handling of UDP command GAME_AVAILABLE unsupporded by server", 0, 0);
                        break;
                }
            }
        }

        /* Check if a remote player is asking for a new connection */
        if (FD_ISSET(serv->sd, &serv->socketFDS)) {
            int connected_sd;
            struct sockaddr_in clientAddress;
            socklen_t fromLength = sizeof(struct sockaddr_in);
            /* Accept player connection if able to */
            bzero(&clientAddress, sizeof(struct sockaddr_in));
            if ((connected_sd = accept(serv->sd, (struct sockaddr *)&clientAddress, &fromLength))) {
                printf("********  TCP Connection  ********\n");
                printf("Connection request from player at %s (port %d)\n", inet_ntoa(clientAddress.sin_addr), clientAddress.sin_port);
                /* Find an open game to assign the connection */
                int gameIndx = find_open_game(serv);
                if (gameIndx >= 0) {
                    /* If an open game was found, assign the connection to the game */
                    struct TTT_Game *currentGame = &serv->gameRoster[gameIndx];
                    printf("Player assigned to Game #%d\n", currentGame->gameNum);
                    currentGame->sd = connected_sd;
                } else {
                    /* If no open games found, close the connection to the remote player */
                    print_error("tictactoe: Unable to find an open game", 0, 0);
                    if (close(connected_sd) < 0) print_error("tistactoe: close-connection", errno, 0);
                }
            } else {
                print_error("accept", errno, 0);
            }
        }

        /* Process received commands from active games */
        for (i = 0; i < MAX_GAMES; i++) {
            struct TTT_Game *currentGame = &serv->gameRoster[i];
            /* Check if game is currently active */
            if (FD_ISSET(currentGame->sd, &serv->socketFDS)) {
                int rv;
                struct TCP_Buffer msg = {0};
                printf("********  Game #%d  ********\n", currentGame->gameNum);
                /* Get the command for the current game */
                if ((rv = get_tcp_command(currentGame->sd, &msg)) > 0) {
                    /* Process received command for current game */
                    commands[(int)msg.command](&msg, currentGame);
                } else {
                    /* Invalid command received -> reset game */
                    reset_game(currentGame);
                }
            }
        }
    }
}
