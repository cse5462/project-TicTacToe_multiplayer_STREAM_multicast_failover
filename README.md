# TicTacToe Program – MultiPlayer - STREAM with Multicast and Failover
> This is the README file for [Final_Project](https://osu.instructure.com/courses/97443/files/27903202/download?download_frd=1)

**NAME:** Conner Graham  
**DATE:** 04/29/2021

## Table of Contents:
- [Included Files](#included-files)
- [TicTacToe Server](#tictactoe-server)
  - [Description](#description-server)
  - [Usage](#usage-server)
  - [Assumptions](#assumptions-server)
- [TicTacToe Client](#tictactoe-client)
  - [Description](#description-client)
  - [Usage](#usage-client)
  - [Assumptions](#assumptions-client)

## Included Files
- [makefile](https://github.com/CSE-5462-Spring-2021/project-1-conner-ben/blob/main/makefile)
- Server (Player 1) Design Document - [Design_Server.md](https://github.com/CSE-5462-Spring-2021/project-1-conner-ben/blob/main/Design_Server.md)
- TicTacToe Server Source Code - [tictactoeServer.c](https://github.com/CSE-5462-Spring-2021/project-1-conner-ben/blob/main/tictactoeServer.c)
- Client (Player 2) Design Document - [Design_Client.md](https://github.com/CSE-5462-Spring-2021/project-1-conner-ben/blob/main/Design_Client.md)
- TicTacToe Client Source Code - [tictactoeClient.c](https://github.com/CSE-5462-Spring-2021/project-1-conner-ben/blob/main/tictactoeClient.c)

## TicTacToe Server
> By: Conner Graham

### DESCRIPTION <a name="description-server"></a>
This lab contains a program called "tictactoeServer" which sets up
multiple net-enabled TicTacToe games that clients can play simultaneously.
This server sets up a communication endpoint for other players to communicate
with, initializes a set of game boards, and processes any commands received
from other players. These commands can include initializing or resuming a game
of TicTacToe when a player requests one, responding to other player's moves
until a winner is found or the game is a draw, or ending a game of TicTacToe
when a player requests to. If a player leaves the game (i.e. closes the
connection) the game is reset for another player to play.
The specific tasks the server performs are as follows:
- Create and bind sockets to send/receive messages for the serevr multicast group
- Create and bind server socket from user provided port
- Print server info and listen for client connections
- Initialize all game boards
- Accept UDP DGRAM messages from the server multicast group
- Process the command for multicast group
- Accept TCP STREAM connections and messages from waiting clients
- Process the command for the corresponding game

If the number of arguments is incorrect or the remote port is
invalid, the program prints appropriate messages and shows how to
correctly invoke the program. 

### USAGE <a name="usage-server"></a>
Start the TicTacToe P1 Server with the command...
```sh
$ tictactoeServer <local-port>
```

If any of the argument strings contain whitespace, those
arguments will need to be enclosed in quotes.

### ASSUMPTIONS <a name="assumptions-server"></a>
- It is assumed that the maximum size a buffer could be is 100 bytes.
- It is assumed that the Player 1 (server) will terminate the server
  when they are done playing, in which case clients will need to
  find another serevr to connect to if they sich to play.
- It is assumed that the client will never enter the int -1 (char '/')
  for a move as it is being used as an error code.
- It is assumed that the multicast group will be at the IP address
  239.0.0.1 on port 1818.
- It is assumed that the 9 bytes suppling the board state will be
  immediately following the RESUME_GAME command.  

## TicTacToe Client
> By: Conner Graham

### DESCRIPTION <a name="description-client"></a>
This lab contains a program called "tictactoeClient" which creates and sets up a 5 byte transfer protocal client. This client connects to the specified server( IP address and port). Next,this client sends 5 bytes of data to the specified server( IP address and port) and then reads in 5 bytes of data from the server. This process continues until a winner or a tie has been reached.

The specific tasks the client performs are as
follows:
- Connect to the Server
- Create server socket from user provided IP/port
- Send Stream Socket connection requests to the server
- Process recieved data
- Terminate the connection to the server

### USAGE <a name="usage-client"></a>
Start the TicTacToe P2 Client with the command...
```sh
$ tictactoeClient <local-port> <remote-IP>
```

If any of the argument strings contain whitespace, those
arguments will need to be enclosed in quotes.

### ASSUMPTIONS <a name="assumptions-client"></a>
- Client send and recieves a 5 bytes 
- The spaces on the tictactoe board are 1-9
- Player 1 is the "server": they are the one who calls bind()   
- Player 1 goes first
- On any errors, close the connection 
- It is assumed that the IP addresses 0.0.0.0 and 255.255.255.255 are invalid remote server addresses to connect to as they are reserved values.
