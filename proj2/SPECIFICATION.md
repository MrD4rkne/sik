# The Great Approximator

In this task, we implement server and client programs for a game titled "The Great Approximator". The goal of each player (represented by a client program) is to approximate as well as possible a polynomial f(x)=∑Ni=0aixi of degree N received from the server at points that are integers from 0 to K. Initially, the values of the approximating function f^ are zero. Approximation is done by adding a given value at a point specified by the client, e.g., for K=3 and addition commands:

- 4.5 at point 0,
- 2 at point 2,
- 3.75 at point 2,

the values of the approximating function are respectively:

f^(0)=4.5, f^(1)=0, f^(2)=5.75, f^(3)=0.

The player's score is ∑Kx=0(f^(x)−f(x))2, i.e., the sum of squares of deviations from the expected value. Penalties described below are added to this value.

The game ends after the server executes M additions.

## Server and Client Launch Parameters

We run the server with the command approx-server with the following parameters:

- `-p port` - server port number, integer from 0 to 65535 written in base 10, optional parameter, default 0;
- `-k value` – value of constant K, integer from 1 to 10000 written in base 10, optional parameter, default 100;
- `-n value` – value of constant N, integer from 1 to 8 written in base 10, optional parameter, default 4;
- `-m value` – value of constant M, integer from 1 to 12341234 written in base 10, optional parameter, default 131;
- `-f file` – name of file containing server messages with polynomial coefficients (see COEFF message description), mandatory parameter.

The server listens on the given port for TCP connections from clients. If the port number is zero, the server chooses any port. It should be possible to establish both IPv6 and IPv4 connections with the server. If the computer on which the server is running does not have IPv6 communication capability, IPv4 communication should be possible.

We run the client with the command approx-client with the following parameters:

- `-u player_id` - player identifier consisting of digits and lowercase and uppercase letters of the English alphabet, mandatory parameter;
- `-s server` – server address or name, mandatory parameter;
- `-p port` – server port, integer from 1 to 65535 written in base 10, mandatory parameter;
- `-4` – force communication with server using IPv4, optional parameter;
- `-6` – force communication with server using IPv6, optional parameter;
- `-a` – choice of strategy type (see "Client Strategy" section), optional parameter.

If both parameters -4 and -6 are given or neither is given, the client should choose the protocol version resulting from the first IP address assigned to the server (see getaddrinfo function).

Parameters can be given in any order. The behavior of the program when any parameter is given multiple times should be reasonable.

## Game Flow

### Conventions

Communication between client and server is textual over a TCP connection using IPv4 or IPv6. Each message ends with a carriage return (ASCII code 13) and newline (ASCII code 10) sequence, denoted as \r\n below. Message fields are separated by a single space. Messages contain no other whitespace characters. Message fields with variable values have names starting with a dollar sign $.

Whenever rational numbers are mentioned in the task description, we transmit them written in base ten with an optional fractional part separated by a dot. After the dot, there can be at most 7 digits. Examples of correct numbers are: 42, 12.78, -123.456789, 1.0, -2., 0.

### Player Registration

The server waits for TCP connections. The client opens a connection to the server and sends the message:

HELLO $player_id\r\n

Player identifiers can repeat. The server must not confuse clients, even if they have identical player identifiers.

If the server does not receive a HELLO message within 3 seconds of client connection, it disconnects from the client and removes all remembered data associated with that client.

The HELLO message can only occur as the first message from the client after establishing a connection with the server.

In response to the HELLO message, the server immediately replies with the message:

COEFF $a_0 $a_1 ... $a_N\r\n

where $a_0, $a_1, …, $a_N are rational numbers in decimal expansion, and indicates that this client should create an approximation of polynomial f(x)=∑Ni=0aixi. For this purpose, the server reads the next line of text from the file whose name was given in parameter -f. We can assume that each line of this file contains a correct COEFF message ending with \r\n sequence and that the file contains a sufficient number of such messages. Each number $a_... fits in the closed interval from −100 to 100.

The COEFF message can only occur as the first message from server to client.

### Game

The game begins when any client sends the command:

PUT $point $value\r\n

where $point is an integer from 0 to K, and $value is a rational number in decimal expansion from the closed interval from −5 to 5.

If either $point or $value is incorrect, the server waits 1 second and responds to the client with the message:

BAD_PUT $point $value\r\n

with repeated values $point and $value from the PUT message and adds a penalty of 10 to the client.

If values $point and $value are correct, the server adds value $value at point $point to the approximating function of the client that sent this message, and in response sends the client the current values of their approximating function, delaying the response by as many seconds as there are lowercase letters in the player identifier, sending the message:

STATE $r_0 ... $r_K\r\n

where rx=f^(x) is the value of the approximating function at point x.

It is not allowed to send a PUT message before receiving a COEFF message and before receiving a response to the previous PUT message. If the server detects such a situation, it immediately sends the message:

PENALTY $point $value\r\n

and adds a penalty of 20 to the client.

During the game, additional clients can join.

### Game End

A client can disconnect from the server at any time. In such a situation, the server removes all remembered data associated with that client and continues operation as if that client never existed. This also applies to counting PUT messages below.

When clients send a total of M correct PUT messages, the server ends the game by sending to all clients a message containing a list with player identifiers and their achieved scores:

SCORING $player_id_1 $result_1 $player_id_2 $result_2 ...\r\n

Player identifiers in the above message should be sorted lexicographically in ascending order by ASCII codes. Player scores are rational numbers in decimal expansion.

After sending the SCORING message, the server disconnects all clients, removes all remembered data associated with clients, waits 1 second, and starts working from the beginning. The position in the coefficients file is not reset. After receiving the SCORING message, the client terminates with code 0.

If the server disconnects without sending a SCORING message, the client prints diagnostic information: ERROR: unexpected server disconnect\n and terminates with code 1.

## Client Strategy

Two client operation modes should be implemented:

1. A client run with option -a implements an automatic game strategy and sends PUT commands itself; the strategy should be better than random;
2. A client run without option -a reads pairs of numbers from standard input, each pair of numbers causes sending a PUT command, the first of these numbers is $point, and the second $value; reading from standard input must not block communication with the server. The PUT command is sent immediately after reading the numbers, regardless of whether we received a response to the previous PUT message. If the user enters numbers before receiving the COEFF message, we send them after receiving that message. Pairs of numbers read from standard input are separated by single spaces. Lines end standardly (with \n character), and numbers are formatted the same as rational numbers in the task. If a line is not correct, we print an error message: ERROR: invalid input line $entire_incorrect_line and continue processing the next line.

## Error Handling

Programs should carefully check parameter correctness, and error information should be printed to standard error output, terminating the program with code 1.

Errors related to calling system or library functions that prevent further program operation should be handled by printing appropriate information to standard error output and terminating the program with code 1.

Information printed to standard error output should have the format:

ERROR: $error_description\n

We do not specify exactly the content of the $error_description field, but it should contain a reasonable error message.

If the server or client receives an incorrect or unexpected message $message, it prints to standard error output the information:

ERROR: bad message from [$ip_address]:$port, $player_id: $message\n

Fields $ip_address and $port contain respectively the IP address and port number of the message sender. In particular, if the message contains a decimal number written inconsistently with the specification in the "Conventions" section, then it is incorrect. If $player_id is not known, we insert UNKNOWN instead.

If the first message from the client is not correct, the server prints the above-described diagnostic information and closes the connection with that client. If the first message from the server is not correct, the client prints the above-described diagnostic information, closes the connection with the server, and terminates with code 1. In other cases, an incorrect message, besides printing information about it, is ignored.

## Diagnostic Information

Programs print information about game progress to standard output.

### Server

For example, the server prints the following information to standard output:

New client [$ip]:$port.
$ip:$port is now known as $player_id.
$player_id get coefficients $a_0 ... $a_N.
$player_id puts $value in $point, current state $r_0 ... $r_K.
Sending state $r_0 ... $r_K to $player_id.
Game end, scoring: $player_id_1 $result_1 $player_id_2 $result_2 ...

### Client

For example, the client prints the following information to standard output:

Connected to [$ip]:$port.
Received coefficients $a_0 ... $a_N.
Putting $value in $point.
Received state $r_0 ... $r_K.
Game end, scoring: $player_id_1 $result_1 $player_id_2 $result_2 ...

## Additional Requirements

The server handles multiple clients simultaneously. Communication with a given client should not block communication with other clients. Client connection and disconnection should not affect communication with other clients. Message format compliance with the specification should be carefully checked. Client expectations of the server and vice versa are defined by the communication protocol. The client may be tested with a server that uses different delays than the one in the task description. Incorrect operation of one client must not affect handling of other clients.

Using the double type is sufficient for floating-point calculations. There is no need to handle overflow.

## Solution

The solution should be implemented in C or C++, using the socket interface. No other libraries implementing network communication may be used. Programs must compile and run in the computer laboratory both on the students machine and on machines in the labs.

Programs should be written according to best practices. The lack of obvious expectations for programs (e.g., that they won't format the disk, or that return values of system functions are checked) in the task description does not mean that a program that doesn't meet them will be considered good. The program code will also be subject to evaluation. The protocol will be tested rigorously.

As a solution, submit an archive containing files necessary to build the solution. Binary files and other unnecessary files must not be included. Use zip, rar, 7z, or tar and gzip programs to create the archive. The archive should have the appropriate extension .zip, .rar, .7z, or .tgz respectively. After unpacking, all files should be in the directory where the archive file is. The archive cannot contain subdirectories. The archive should contain a makefile or Makefile. Running the make command should create executable files approx-server and approx-client. Running the make clean command should remove all files created during compilation.
