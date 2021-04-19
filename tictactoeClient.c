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

/* Structure to send and recieve UCP player messages. */
struct UDP_Buffer {
    char version;           // version number
    char command;           // player command
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

/*****************************/
/* GENERAL PURPOSE FUNCTIONS */
/*****************************/

/* The number of command line arguments. */
#define NUM_ARGS 3
/* The maximum size of a buffer for the program. */
#define BUFFER_SIZE 100
/* The number of seconds spend waiting before multicast group times out. */
#define MC_TIMEOUT 30
/* The number of attempts before giving up on multicast group. */
#define MC_ATTEMPTS 5
/* The error code used to signal an invalid move. */
#define ERROR_CODE -1

void print_error(const char *msg, int errnum, int terminate);
void handle_init_error(const char *msg, int errnum);
void extract_args(char *argv[], int *port, unsigned long *address);

/********************************/
/* SOCKET AND NETWORK FUNCTIONS */
/********************************/

/* The protocol version number used. */
#define VERSION 6
/* The port number for the multicast group. */
#define MC_PORT 1818
/* The network IP address for the multicast group. */
#define MC_GROUP "239.0.0.1"

int create_endpoint(struct sockaddr_in *socketAddr, int type, unsigned long address, int port);
void get_new_server(int mcd, const struct sockaddr_in *groupAddr, int *sd, int resume);
void set_timeout(int sd, int seconds);
void print_client_info();

/******************************/
/* TIC-TAC-TOE GAME FUNCTIONS */
/******************************/

/* The baord marker used for Player 1 */
#define P1_MARK 'X'
/* The baord marker used for Player 2 */
#define P2_MARK 'O'

void init_game(int sd, struct TTT_Game *game);
int get_udp_command(int sd, struct sockaddr_in *playerAddr, struct UDP_Buffer *datagram);
void send_request_game(int mcd, const struct sockaddr_in *groupAddr);
int get_tcp_command(int sd, struct TCP_Buffer *msg);
void send_new_game(struct TTT_Game *game);
void send_game_over(struct TTT_Game *game);
void send_resume_game(struct TTT_Game *game);
int get_move(const struct TTT_Game *game);
int validate_move(int choice, const struct TTT_Game *game);
int send_p2_move(const struct TTT_Game *game);
int check_win(const struct TTT_Game *game);
int check_draw(const struct TTT_Game *game);
int check_game_over(struct TTT_Game *game);
void print_board(const struct TTT_Game *game);
void leave_game(struct TTT_Game *game);
void tictactoe(int mcd, const struct sockaddr_in *groupAddr, int sd);

/*******************/
/* PLAYER COMMANDS */
/*******************/

/* The size (in bytes) of each UDP game command. */
#define UDP_CMD_SIZE 2
/* The size (in bytes) of each TCP game command. */
#define TCP_CMD_SIZE 4

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
void game_available(int mcd, const struct sockaddr_in *groupAddr, int *sd, struct sockaddr_in *serverAddr);

/**
 * @brief This program creates and sets up a TicTacToe client which acts as Player 2 in a
 * 2-player game of TicTacToe. This client creates a multicast socket to message the multicast
 * group if there is a connection issue with the server and a server socket for the client to
 * connect to, connects to the specified server if able, otherwise messages the server group for
 * a new server, and then initiates a simple game of TicTacToe in which Player 1 and Player 2
 * take turns making moves which they send to the other player. If an error occurs before a valid
 * connection is established, the program terminates and prints appropriate error messages,
 * otherwise an error message is printed and the error is handled appropriately.
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
    int mcd, sd, portNumber;
    unsigned long serverNetAddr;
    struct sockaddr_in multicastAddr, serverAddr;

    /* If arg count correct, extract arguments to their respective variables */
    if (argc != NUM_ARGS) handle_init_error("argc: Invalid number of command line arguments", 0);
    extract_args(argv, &portNumber, &serverNetAddr);

    /* Print client information  */
    print_client_info();

    /* Create multicast socket and set timeout for multicast group */
    mcd = create_endpoint(&multicastAddr, SOCK_DGRAM, inet_addr(MC_GROUP), MC_PORT);
    printf("Communication endpoint for multicast group at %s (port %hu)\n", inet_ntoa(multicastAddr.sin_addr), multicastAddr.sin_port);
    set_timeout(mcd, MC_TIMEOUT);

    /* Create server socket to connect to */
    sd = create_endpoint(&serverAddr, SOCK_STREAM, serverNetAddr, portNumber);
    /* Find server to connect to if initial server fails */
    printf("Attempting to connect to server...\n");
    if (connect(sd, (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in)) != -1) {
        printf("Connected to server at %s (port %hu)\n", inet_ntoa(serverAddr.sin_addr), serverAddr.sin_port);
    } else {
        print_error("connect", errno, 0);
        get_new_server(mcd, &multicastAddr, &sd, 0);
    }
    /* Start the game of TicTacToe */
    tictactoe(mcd, &multicastAddr, sd);

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
    printf("Usage is: tictactoeClient <remote-port> <remote-IP>\n");
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
 * @param port The remote port number that the client will communicate on
 * @param address The remote IP address of the server
 */
void extract_args(char *argv[], int *port, unsigned long *address) {
    /* Extract and validate remote port number */
    *port = strtol(argv[1], NULL, 10);
    if (*port < 1 || *port != (u_int16_t)(*port)) handle_init_error("extract_args: Invalid port number", 0);
    /* Extract and validate remote IP address */
	*address = inet_addr(argv[2]);
	if (*address == INADDR_NONE || *address == INADDR_ANY) handle_init_error("remote-IP: Invalid server address", 0);
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
        /* Clear the initial socket address structure */
        bzero((char *)socketAddr, sizeof(struct sockaddr_in *));
        /* Assign address family to socket */
        socketAddr->sin_family = AF_INET;
        /* Assign IP address to socket */
        socketAddr->sin_addr.s_addr = address;
        /* Assign port number to socket */
        socketAddr->sin_port = htons(port);
    } else {
        print_error("create_endpoint: socket", errno, 1);
    }
    /* Check socket type if successful */
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
 * @brief Messages the multicast server group for a new server that is available for the
 * client to connect to.
 * 
 * @param mcd The socket descriptor of the multicast group.
 * @param groupAddr The address of the multicast group.
 * @param sd The socket descriptor of the server comminication endpoint.
 * @param resume Whether or not a game is being resumed or started.
 */
void get_new_server(int mcd, const struct sockaddr_in *groupAddr, int *sd, int resume) {
    int rv;
    struct sockaddr_in clientAddress;
    struct UDP_Buffer datagram = {0};

    /* Closes the previous server connection if it was still open */
    if (sd >= 0) {
        if (close(*sd) < 0) print_error("leave_game: close-connection", errno, 0);
    }
    /* Message server group for new server to connect to */
    send_request_game(mcd, groupAddr);
    while ((rv = get_udp_command(mcd, &clientAddress, &datagram)) <= 0) {
        if (rv == 0) exit(0);
    }
    /* Process received command */
    switch (datagram.command) {
        case REQUEST_GAME:
            print_error("tictactoe: handling of UDP command GAME_AVAILABLE unsupporded by server", 0, 0);
        case GAME_AVAILABLE:
            game_available(mcd, groupAddr, sd, &clientAddress);
            break;
    }
}

/**
 * @brief Sets the time to wait before a timeout on recvfrom calls to the specified number
 * of seconds, or turns it off if zero seconds was entered.
 * 
 * @param sd The socket descriptor of the comminication endpoint.
 * @param seconds The number of seconds to wait before a timeout.
 */
void set_timeout(int sd, int seconds) {
    struct timeval time = {0};
    time.tv_sec = seconds;

    /* Sets the recvfrom timeout option */
    if (setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time)) < 0) {
        print_error("set_timeout", errno, 0);
    }
}

/**
 * @brief Prints the client network information.
 * 
 */
void print_client_info() {
    int hostname;
    char hostbuffer[BUFFER_SIZE], *IP_addr;
    struct hostent *host_entry;

    /* Retrieve the hostname */
    if ((hostname = gethostname(hostbuffer, sizeof(hostbuffer))) == -1) {
        print_error("print_client_info: gethostname", errno, 1);
    }
    /* Retrieve the host information */
    if ((host_entry = gethostbyname(hostbuffer)) == NULL) {
        print_error("print_client_info: gethostbyname", errno, 1);
    }
    /* Convert the host internet network address to an ASCII string */
    IP_addr = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));
    /* Print the IP address for the client */
    printf("[+]Established client at %s\n", IP_addr);
}

/**
 * @brief Initializes the starting state of the game.
 * 
 * @param sd The socket descriptor of the connected player's comminication endpoint.
 * @param game The current game of TicTacToe being played.
 */
void init_game(int sd, struct TTT_Game *game) {
    int i;
    printf("[+]Initializing shared game state.\n");
    /* Initialize game attributes */
    game->sd = sd;
    game->gameNum = -1;
    game->winner = -1;
    /* Initializes the shared state (aka the board)  */
    for (i = 1; i <= sizeof(game->board); i++) {
        game->board[i-1] = i + '0';
    }
}

/**
 * @brief Gets a UDP command from the remote player and attempts to validate the data and
 * syntax based on the current protocol.
 * 
 * @param sd The socket descriptor of the connected player's comminication endpoint.
 * @param playerAddr The address of the connected player.
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
            /* Check if server group has timed out */
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                print_error("get_udp_command: Nobody has responded. Leaving game", 0, 0);
                return 0;
            } else {
                print_error("get_udp_command", errno, 0);
            }
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
 * @brief Sends REQUEST_GAME command to the multicast server group to signal that a new game
 * is needed.
 * 
 * @param mcd The socket descriptor of the multicast group.
 * @param groupAddr The address of the multicast group.
 */
void send_request_game(int mcd, const struct sockaddr_in *groupAddr) {    
    struct UDP_Buffer datagram = {0};
    printf("[+]Contacting server group to request a new game.\n");
    /* Pack command information into datagram */
    datagram.version = VERSION;
    datagram.command = REQUEST_GAME;
    /* Send the command to the remote player */
    printf("Client sent the REQUEST_GAME command to server group\n");
    if (sendto(mcd, &datagram, UDP_CMD_SIZE, 0, (struct sockaddr *)groupAddr, sizeof(struct sockaddr_in)) < 0) {
        print_error("send_request_game", errno, 0);
    }
}

/**
 * @brief Handles the GAME_AVAILABLE command from the remote server. Attempts to connect to
 * the server that sent the command. If a connection is unable to be established, the client
 * messages the multicast server group for a new server.
 * 
 * @param mcd The socket descriptor of the multicast group.
 * @param sd The socket descriptor of the server comminication endpoint.
 * @param serverAddr The address of the server comminication endpoint.
 */
void game_available(int mcd, const struct sockaddr_in *groupAddr, int *sd, struct sockaddr_in *serverAddr) {
    static int attempts = MC_ATTEMPTS;

    printf("Server at %s (port %hu) issued a GAME_AVAILABLE command\n", inet_ntoa(serverAddr->sin_addr), serverAddr->sin_port);
    /* Create server socket to connect to and print client information*/
    *sd = create_endpoint(serverAddr, SOCK_STREAM, serverAddr->sin_addr.s_addr, ntohs(serverAddr->sin_port));
    /* Attempt to connect to the server */
    printf("Attempting to connect to server...\n");
    if (connect(*sd, (struct sockaddr *)serverAddr, sizeof(struct sockaddr_in)) == -1) {
        print_error("game_available: connect", errno, 0);
        if (attempts-- <= 0) print_error("game_available: Maximum attempts to connect to new server exceeded", 0, 1);
        /* Attempt to find new server to connect to */
        get_new_server(mcd, groupAddr, sd, 0);
    } else {
        printf("Connected to server at %s (port %hu)\n", inet_ntoa(serverAddr->sin_addr), serverAddr->sin_port);
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
                print_error("get_tcp_command: Player 1 has disconnected", 0, 0);
                return 0;
            } else {
                print_error("get_tcp_command", errno, 0);
                return ERROR_CODE;
            }
        }
        /* Update total bytes received for message */
        bytes += rv;
    }
    /* Validate the received message */
    if (msg->version != VERSION) {  // check for correct version
        print_error("get_tcp_command: Protocol version not supported", 0, 0);
        return ERROR_CODE;
    } else if (msg->command < NEW_GAME || msg->command > GAME_OVER) {  // check for valid command
        print_error("get_tcp_command: Invalid TCP command", 0, 0);
        return ERROR_CODE;
    }
    return bytes;
}

/**
 * @brief Handles the NEW_GAME command from the remote player. Not supported for Client.
 * 
 * @param msg The message containing the command that the remote player sent.
 * @param game The current game of TicTacToe being played.
 */
void new_game(const struct TCP_Buffer *msg, struct TTT_Game *game) {
    printf("The remote player issued a NEW_GAME command\n");
    print_error("new_game: handling of TCP command NEW_GAME unsupporded by client", 0, 0);
    leave_game(game);
}

/**
 * @brief Sends NEW_GANE command to the remote player to signal start of game.
 * 
 * @param game The current game of TicTacToe being played.
 */
void send_new_game(struct TTT_Game *game) {
    struct TCP_Buffer msg = {0};
    /* Pack command information into message */
    msg.version = VERSION;
    msg.command = NEW_GAME;
    /* Send the command to the remote player */
    printf("Client sent the NEW_GAME command to Player 1\n");
    if (send(game->sd, &msg, TCP_CMD_SIZE, MSG_NOSIGNAL) < 0) {
        print_error("send_new_game", errno, 0);
        leave_game(game);
    }
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
    /* Retrieve game number from remote player if not established */
    if (game->gameNum < 0) game->gameNum = msg->gameNum;
    /* Get move from remote player */
    int move = msg->data - '0';
    printf("The remote player issued a MOVE command\n");
    printf("Player 1 chose the move:  %c\n", msg->data);
    /* Check that the received move is valid */
    if (validate_move(move, game)) {
        /* Update the board (for Player 1) and check if someone won */
        game->board[move-1] = P1_MARK;
        if (check_game_over(game)) {
            /* If Player 1 won, send GAME_OVER command and leave game */
            send_game_over(game);
            return;
        }
        print_board(game);
        /* If nobody won, make a move to send to the remote player */
        if ((move = send_p2_move(game)) == ERROR_CODE) {
            /* Leave game if there was an error sending the move */
            leave_game(game);    
            return;
        }
        /* Update the board (for Player 2) and check if someone won after the exchange */
        game->board[move-1] = P2_MARK;
        check_game_over(game);
    } else {
        leave_game(game);
    }
}

/**
 * @brief Handles the GAME_OVER command from the remote player. Determines the reason for
 * ending the game, prints the appropriate message, and leaves the game.
 * 
 * @param msg The message containing the command that the remote player sent.
 * @param game The current game of TicTacToe being played.
 */
void game_over(const struct TCP_Buffer *msg, struct TTT_Game *game) {
    printf("The remote player issued a GAME_OVER command\n");
    printf("Player 1 has signaled that the game is over\n");
    /* Check if the game is actually over */
    if (game->winner < 0) {
        /* If not over, player decided to leave prematurely */
        print_error("game_over: Game is still in progress", 0, 0);
        printf("Player 1 has decided to leave the game\n");
    } else {
        /* If over, print appropriate message for who won */
        (game->winner == 0) ? printf("==>\a It's a draw\n") : printf("==>\a Player %d wins\n", game->winner);
    }
    /* Leave the game */
    leave_game(game);
}

/**
 * @brief Sends GAME_OVER command to the remote player to signal end of game.
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
    printf("Client sent the GAME_OVER command to Player 1\n");
    if (send(game->sd, &msg, TCP_CMD_SIZE, MSG_NOSIGNAL) < 0) {
        print_error("send_game_over", errno, 0);
    }
    /* Leave the game */
    leave_game(game);
}

/**
 * @brief Handles the RESUME_GAME command from the remote player. Not supported for Client.
 * 
 * @param msg The message containing the command that the remote player sent.
 * @param game The current game of TicTacToe being played.
 */
void resume_game(const struct TCP_Buffer *msg, struct TTT_Game *game) {
    printf("The remote player issued a RESUME_GAME command\n");
    print_error("resume_game: handling of TCP command RESUME_GAME unsupporded by client", 0, 0);
    leave_game(game);
}

/**
 * @brief Sends RESUME_GAME command to the remote player to start of in-progress game.
 * 
 * @param game The current game of TicTacToe being played.
 */
void send_resume_game(struct TTT_Game *game) {
    int i;
    char boardState[GAME_SIZE] = {0};
    struct TCP_Buffer msg = {0};

    /* Reset game number to default state */
    game->gameNum = -1;
    /* Pack command information into message */
    msg.version = VERSION;
    msg.command = RESUME_GAME;
    msg.gameNum = game->gameNum;
    /* Send the command to the remote player */
    printf("Client sent the RESUME_GAME command to Player 1\n");
    if (send(game->sd, &msg, TCP_CMD_SIZE, MSG_NOSIGNAL) < 0) {
        print_error("send_resume_game", errno, 0);
        leave_game(game);
    }
    /* Pack shared board state information into message */
    for (i = 0; i < GAME_SIZE; i++) {
        char state = game->board[i];
        if (state == P1_MARK || state == P2_MARK) {
            boardState[i] = state;
        }
    }
    /* Send the shared board state to the remote player */
    printf("Client sent the current board state to Player 1\n");
    if (send(game->sd, &boardState, GAME_SIZE, MSG_NOSIGNAL) < 0) {
        print_error("send_resume_game", errno, 0);
        leave_game(game);
    }
}

/**
 * @brief Gets the next move the client should make from the user.
 * 
 * @param game The current game of TicTacToe being played.
 * @return The optimal move to make in order to win. 
 */
int get_move(const struct TTT_Game *game) {
    int choice;
    char input[BUFFER_SIZE];
    /* Prompt for next move from user */
    printf("Player 2, enter a number:  ");
    /* Read line of user input */
    fgets(input, sizeof(input), stdin);
    /* Look for integer in input for player's move */
    sscanf(input, "%d", &choice);
    return choice;
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
 * @brief Sends Player 2's move to the remote player.
 * 
 * @param game The current game of TicTacToe being played.
 * @return The move that was sent, or an error code if there was an issue. 
 */
int send_p2_move(const struct TTT_Game *game) {
    struct TCP_Buffer msg = {0};
    /* Get move to send to remote player */
    int move = get_move(game);
    while (!validate_move(move, game)) move = get_move(game);
    /* Pack move information into message */
    msg.version = VERSION;
    msg.command = MOVE;
    msg.data = move + '0';
    msg.gameNum = game->gameNum;
    /* Send the move to the remote player */
    printf("Client sent the move:  %c\n", msg.data);
    if (send(game->sd, &msg, TCP_CMD_SIZE, MSG_NOSIGNAL) < 0) {
        print_error("send_p2_move", errno, 0);
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
 * @brief Closes the connection and leaves the current game.
 * 
 * @param game The current game of TicTacToe being played.
 */
void leave_game(struct TTT_Game *game) {
    /* Checks if game has been initialized */
    if (game->gameNum < 0) {
        printf("Game #? has ended. Leaving the game\n");
    } else {
        printf("Game #%d has ended. Leaving the game\n", game->gameNum);
    }
    /* Close connection to remote player */
    if (close(game->sd) < 0) print_error("leave_game: close-connection", errno, 0);
    exit(EXIT_SUCCESS);
}

/**
 * @brief Plays a game of TicTacToe with a remote player that end when either someone wins,
 * there is a draw, or there is no other server available to connect to if the remote
 * player leaves the game.
 * 
 * @param mcd The socket descriptor of the multicast group.
 * @param groupAddr The address of the multicast group.
 * @param sd The socket descriptor of the connected player's comminication endpoint.
 */
void tictactoe(int mcd, const struct sockaddr_in *groupAddr, int sd) {
    struct TTT_Game game = {0};
    Command_Handler commands[] = {new_game, move, game_over, resume_game};

    /* Initialize the game */
    init_game(sd, &game);
    send_new_game(&game);
    /* Play the game */
    while (1) {
        int rv;
        struct TCP_Buffer msg = {0};
        printf("[+]Waiting for remote player to issue a command...\n");
        /* Get the command for the current game */
        if ((rv = get_tcp_command(game.sd, &msg)) > 0) {
            /* Process received command for current game */
            commands[(int)msg.command](&msg, &game);
        } else if (rv == 0) {
            /* Remote player disconnected -> message server group for new game */
            get_new_server(mcd, groupAddr, &sd, 0);
            /* Resume the game with the new connected player */
            send_resume_game(&game);
        } else {
            /* Invalid command received -> reset game */
            leave_game(&game);
        }
    }
}
