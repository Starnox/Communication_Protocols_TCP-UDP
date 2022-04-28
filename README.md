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
- Nagle algorithm is disabled on each tcp connection (this allows messages to be sent ASAP)
- An application layer over TCP was developed so no information is lost, only the neccesary amount
of bytes are transmitted depening on the format of the packet, portability in mind (Network byte order
was used) and also measures as to be sure a whole packet was recieved and no packets were merged together
in the byte stream -> more details will be given below

Although much of the code is written in C style and uses C functionabilty, I opted for C++ in order
to take advantage of STL data structes (ie map, set, vector) which make life a whole lot easier.

The code passes all the tests given on my local machine and has no memory issues (checked with valgrind).
Also it accomodates for an "unlimited" (bounded of course by memory) amount of clients and messages
using dynamic structures from STl like vectors. 

Also, as a disclaimer, a lot of the principles and the sendall function were learned from this ebook:
https://beej.us/guide/bgnet/html/#listen

---
### Application Layer and Structures
At the base of my protocol is the struct `message` which has the following fields (all in network byte order):
-`uint16_t len len` -> two bytes which will hold the length of the message.
-> being only two bytes they will always be sent together and so we can recieve them first
and then we know how many more bytes we need to recieve in order to construct a whole packet.
Also, by knowing the type of the packet we can adjuts the length of the message accordingly and
only send relevant bytes and by doing so we save on bandwith.
Using this field and the function `sendall()` and `recvall()` we ensure packet integrity.
-`struct in_addr ip` -> 4 bytes for the address of the udp client that sent the packet
-`in_port_t port` -> 2 bytes for the port of the udp client 
-`char topic[TOPIC_LEN]` -> the topic of the message -> max 50 bytes
-`uint8_t type` -> type of the payload (0/1/2/3)  -> 1 byte
-`char payload[PAYLOAD_LEN]` -> the payload of the packet depening on its type it will have variable
number of bytes but a maximum of 1500





