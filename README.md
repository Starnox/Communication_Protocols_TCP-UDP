# Communication Protocols
### Student: Mihailescu Eduard-Florin 322CB

## Overview
This code is meant to solve the second challenge given by the Communication Protocols team
as part of the curriculum studying at Faculty of Automatic Control and Computers 
in Bucharest. 
As part of the challenge, I implemented client-server functionability using TCP and UDP protocols.
The server acts as a point of relay which recieves informations from UDP clients 
(ex. temperature sensors that give information regarding the temperature in a room) and then forwards
the messages recieved to all the clients that "want" to recieve the message (i.e they are subscribed
to the topic on which the udp sent the message). 

The application also has a "store-forward" feature
which enables the server to store messages for clients which are not connected but want to recieve 
information from a specific topic (sf = 1) and send them as soon as it reconnects. 
The application is built with speed and correctness in mind and to accomodate that, the following 
measures were taken:
-   Nagle algorithm is disabled on each tcp connection (this allows messages to be sent ASAP)
-   An application layer over TCP was developed so no information is lost, only the neccesary amount
of bytes are transmitted depening on the format of the packet, portability in mind (Network byte order
was used) and also measures as to be sure a whole packet was recieved and no packets were merged together
in the byte stream -> more details will be given below

Although much of the code is written in C style and uses C functionabilty, I opted for C++ in order
to take advantage of STL data structes (ie map, set, vector) which make life a whole lot easier.

The code passes all the tests given on my local machine and has no memory issues (checked with valgrind).
Also it accomodates for an "unlimited" (bounded of course by memory) amount of clients and messages
using dynamic structures from STl like vectors. 

Also, as a disclaimer, a lot of the principles and the sendall function were learned from this ebook:
https://beej.us/guide/bgnet/html/#listen and as a start point I used the solution to lab 8

---
### Application Layer and Structures
At the base of my protocol is the struct `message` which has the following fields (all in network byte order):
-`uint16_t len len` -> two bytes which will hold the length of the message.
-> being only two bytes they will always be sent together and so we can recieve them first
and then we know how many more bytes we need to recieve in order to construct a whole packet.
Also, by knowing the type of the packet we can adjuts the length of the message accordingly and
only send relevant bytes and by doing so we save on bandwith.
Using this field and the function `sendall()` and `recvall()` we ensure packet integrity.
-   `struct in_addr ip` -> 4 bytes for the address of the udp client that sent the packet
-   `in_port_t port` -> 2 bytes for the port of the udp client 
-   `char topic[TOPIC_LEN]` -> the topic of the message -> max 50 bytes
-   `uint8_t type` -> type of the payload (0/1/2/3)  -> 1 byte
-   `char payload[PAYLOAD_LEN]` -> the payload of the packet depening on its type it will have variable
number of bytes but a maximum of 1500

The tcp subscriber can also sent commands to the server -> these are encapsulated in the client_command
structure which is very similar with the one above only this one contains 
just the length and the paylaod.

### server
At first we initialise all the structures we will need and create the initial sockets.
The main structures the server uses are the following:
`std::vector<TCP_Client*> subscribers` -> a vector with pointers the the clients that have connected 
so far (clients with unique id)
`std::map<int, TCP_Client*> fd_to_client` -> a relation between a socket(fd) and a subscriber
`std::map<std::string, std::set<TCP_Client *>>` -> relation between a topic and the subscribers
to that topic
`std::map<std::pair<TCP_Client*,std::string>,bool>` -> a relation between a pair of client and topic
and if it has store forward activated for that topic
`std::map<TCP_Client *, std::vector<char *>> to_send` -> "database" of messages that need to be
sent as soon as a client connects to the server

After the tcp and udp listening sockets are created, the main loop of the program starts
Using `select` the app multiplexes the information recieved from the clients and from stdin.
Depending on the type and the input we recieve we do different things:
-   If we recieve a new tcp connection request we call `incoming_tcp_connection()`
-   If we recieve information on the udp socket we call `incoming_udp_datagram()`
-   If the user types exit, then we clean up the memory and close all connections
-   If the user sends data, we process it and modify the structures accordingly

`check_server_commands()` -> Verify if the user typed "exit"
`incoming_tcp_connection()` ->
-   Accept the connection
-   Get the id as a small payload(only 10 bytes -> we can just use recv())
-   Go through the list of known subscribers and check if the subscriber with the given id
is already connected, if it is display a message, otherwise mark the client as connected,
add it again to the set of fds, map the fd the client and check if has messages
-   If the client doesn't exist in the "database", then create it, make the connections,
disable the nagle algorithm for the socket, send a message to the client that marks it connected 
succesfully and then send it the messages it missed.

`incoming_udp_datagram()` ->
-   Use recvfrom to get the datagram from the socket
-   Extract the information and store it in a message structure
-   Calculate length of the packet
-   Convert to Network byte order
-   If the client is connected, send the message using `sendall()`
-   If the client is not connected, store the message in order to be sent later


### subscriber
The beginning is similar to the server's. It tries to connect to server given as an argument
with the id also given by an argument using `check_id()`. If it succedes then the main loop
starts with also uses `select()` to multiplex the information recieved:
-   If it recieves data on stdin, then it sanitize the input in verifies that the command is valid
-   If it is an exit command, then just exit the program and close the connection
-   If it is a subscribe or unsubscribe command then it creates a client_command structure,
fills it up accordingly and sends it to the server using `sendall()`
-   If it recievs data from the server then we process it and display with `format_message()`


`check_id()` -> Send the id the server to check if it is available and depending on the response
it return true or false

`format_message()` -> Depending on the type of the payload recieved, it extracts and formats the
data as shown in the challenge statement, then prints it to the console.


### helpers.c and helpers.h
Helper function and structures used by both the client and the server

`disable_nagle()` -> disable the nagle algorithm for the given socket

`sendall()` -> sends all the bytes in the buffer in a while loop

`recvall()` -> first get the length of the message, then while the number of bytes recieved
is smaller then that length, continues to recieve.

### tcp_client.cpp and tcp_client.h
A class that represent a client with the following fields:
-   `string id`
-   `int fd`
-   `bool connected`








