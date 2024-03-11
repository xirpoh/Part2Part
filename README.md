# Part2Part

Peer-to-peer multithreaded file sharing application written in C using the TCP/IP protocol.

The application uses a hybrid system, which consists of a server program that can be launched by an author, who is also responsible for starting/stopping the service, and a client (peer) program that can be run by any user. The role of the server is to receive and subsequently execute requests from the clients.

# Server

The server waits for a client to connect and will create a thread that will take over communication with it, while the server will continue to listen for further connections. The created thread will wait for requests from the client, which it will then execute.

# Client/Peer

The initial peer program connects to the server as a client and will create
a child process. Once connected to the server, the application user will be able to enter commands, which are sent to the server. Then a response from the server is received and the process is repeated. The child's process purpuse is running its own concurrent server in the background designed to handle the transfer requests from the other peers in parallel.
