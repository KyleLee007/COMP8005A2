/*-----------------------------------------------------------------------------
 --	SOURCE FILE:    epollServer.c - A simple epoll server program
 --
 --	PROGRAM:		Web Client Emulator
 --
 --	FUNCTIONS:		
 --                 int main(int argc, char **argv);
 --                 void server(int port, int comm);
 --                 int processConnection(int socket, int comm);
 --                 void initializeServer(int *listenSocket, int *port);
 --                 void displayClientData(unsigned long long clients);
 --                 static void systemFatal(const char *message);
 --
 --	DATE:			February 8, 2012
 --
 --	REVISIONS:		(Date and Description)
 --
 --	DESIGNERS:      Luke Queenan
 --
 --	PROGRAMMERS:	Luke Queenan
 --
 --	NOTES:
 -- A simple epoll server
 ----------------------------------------------------------------------------*/

/* System includes */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <unistd.h>

/* User includes */
#include "network.h"

#define MAX_EVENTS 10000

int main(int argc, char **argv);
void server(int port, int comm);
int processConnection(int socket, int comm);
void initializeServer(int *listenSocket, int *port);
void displayClientData(unsigned long long clients);
static void systemFatal(const char *message);

typedef struct
{
    int socket;
    int commSocket;
} clientData;

/*
 -- FUNCTION: main
 --
 -- DATE: Feb 20, 2011
 --
 -- REVISIONS: (Date and Description)
 --
 -- DESIGNER: Luke Queenan
 --
 -- PROGRAMMER: Luke Queenan
 --
 -- INTERFACE: int main(argc, char **argv)
 --
 -- RETURNS: 0 on success
 --
 -- NOTES:
 -- This is the main entry point for the epoll server
 */
int main(int argc, char **argv)
{
    /* Initialize port and give default option in case of no user input */
    int port = DEFAULT_PORT;
    int option = 0;
    int comms[2];
    
    /* Parse command line parameters using getopt */
    while ((option = getopt(argc, argv, "p:")) != -1)
    {
        switch (option)
        {
            case 'p':
                port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -p [port]\n", argv[0]);
                return 0;
        }
    }
    
    /* Create the socket pair for sending data for collection */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, comms) == -1)
    {
        systemFatal("Unable to create socket pair");
    }
    
    /* Need to fork and create process to collect data */
    
    /* Start server */
    server(port, comms[1]);
    
    return 0;
}

/*
 -- FUNCTION: server
 --
 -- DATE: Feb 20, 2011
 --
 -- REVISIONS: (Date and Description)
 --
 -- DESIGNER: Luke Queenan
 --
 -- PROGRAMMER: Luke Queenan
 --
 -- INTERFACE: void server(int, int)
 --
 -- RETURNS: void
 --
 -- NOTES:
 -- This function contains the server loop for epoll. It accept client
 -- connections and calls the process connection function when a socket is ready
 -- for reading.
 */
void server(int port, int comm)
{
    register int epoll = 0;
    register int ready = 0;
    register int index = 0;
    int listenSocket = 0;
    int client = 0;
    
    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];
    unsigned long long connections = 0;
    
    /* Initialize the server */
    initializeServer(&listenSocket, &port);
    
    /* Set up epoll variables */
    if ((epoll = epoll_create1(0)) == -1)
    {
        systemFatal("Unable to create epoll object");
    }
    
    event.events = EPOLLIN;
    event.data.fd = listenSocket;
    
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, listenSocket, &event) == -1)
    {
        systemFatal("Unable to add listen socket to epoll");
    }
    
    displayClientData(connections);
    
    while (1)
    {
        /* Wait for epoll to return with the maximum events specified */
        ready = epoll_wait(epoll, events, MAX_EVENTS, -1);
        if (ready == -1)
        {
            systemFatal("Epoll wait error");
        }
        
        /* Iterate through the returned sockets and deal with them */
        for (index = 0; index < ready; index++)
        {
            if (events[index].data.fd == listenSocket)
            {
                /* Accept the new connections */
                while ((client = acceptConnection(&listenSocket)) != -1)
                {
                    if (makeSocketNonBlocking(&client) == -1)
                    {
                        systemFatal("Cannot make client socket non-blocking");
                    }
                    event.events = EPOLLIN | EPOLLET;
                    event.data.fd = client;
                    if (epoll_ctl(epoll, EPOLL_CTL_ADD, client, &event) == -1)
                    {
                        systemFatal("Cannot add client socket to epoll");
                    }
                    connections++;
                    displayClientData(connections);
                }
            }
            else
            {
                if (processConnection(events[index].data.fd, comm) == 0)
                {
                    close(events[index].data.fd);
                    connections--;
                    displayClientData(connections);
                }
            }
        }
    }
    
    close(listenSocket);
    close(epoll);
}

/*
 -- FUNCTION: processConnection
 --
 -- DATE: Feb 20, 2011
 --
 -- REVISIONS: (Date and Description)
 --
 -- DESIGNER: Luke Queenan
 --
 -- PROGRAMMER: Luke Queenan
 --
 -- INTERFACE: int processConnection(int, int)
 --
 -- RETURNS: 1 on success
 --
 -- NOTES:
 -- Service a client socket by reading a request and sending the data to the
 -- client.
 */
int processConnection(int socket, int comm)
{
    int bytesToWrite = 0;
    char line[NETWORK_BUFFER_SIZE];
    char result[NETWORK_BUFFER_SIZE];
    
    /* Ready the memory for sending to the client */
    memset(result, 'L', NETWORK_BUFFER_SIZE);
    
    /* Read the request from the client */
    if (readLine(&socket, line, NETWORK_BUFFER_SIZE) <= 0)
    {
        return 0;
    }
    
    /* Get the number of bytes to reply with */
    bytesToWrite = atol(line);
    
    /* Ensure that the bytes requested are within our buffers */
    if ((bytesToWrite <= 0) || (bytesToWrite > NETWORK_BUFFER_SIZE))
    {
        systemFatal("Client requested too large a file");
    }

    /* Send the data back to the client */
    if (sendData(&socket, result, bytesToWrite) == -1)
    {
        systemFatal("Send fail");
    }

    /* Send the communication time to the data collection process */
    
    
    return 1;
}

/*
 -- FUNCTION: initializeServer
 --
 -- DATE: March 12, 2011
 --
 -- REVISIONS: September 22, 2011 - Added some extra comments about failure and
 -- a function call to set the socket into non blocking mode.
 --
 -- DESIGNER: Luke Queenan
 --
 -- PROGRAMMER: Luke Queenan
 --
 -- INTERFACE: void initializeServer(int *listenSocket, int *port);
 --
 -- RETURNS: void
 --
 -- NOTES:
 -- This function sets up the required server connections, such as creating a
 -- socket, setting the socket to reuse mode, binding it to an address, and
 -- setting it to listen. If an error occurs, the function calls "systemFatal"
 -- with an error message.
 */
void initializeServer(int *listenSocket, int *port)
{
    // Create a TCP socket
    if ((*listenSocket = tcpSocket()) == -1)
    {
        systemFatal("Cannot Create Socket!");
    }
    
    // Allow the socket to be reused immediately after exit
    if (setReuse(listenSocket) == -1)
    {
        systemFatal("Cannot Set Socket To Reuse");
    }
    
    // Bind an address to the socket
    if (bindAddress(port, listenSocket) == -1)
    {
        systemFatal("Cannot Bind Address To Socket");
    }
    
    if (makeSocketNonBlocking(listenSocket) == -1)
    {
        systemFatal("Cannot Make Socket Non-Blocking");
    }
    
    // Set the socket to listen for connections
    if (setListen(listenSocket) == -1)
    {
        systemFatal("Cannot Listen On Socket");
    }
}

void displayClientData(unsigned long long clients)
{
    printf("Connected clients: %llu\n", clients);
}

/*
 -- FUNCTION: systemFatal
 --
 -- DATE: March 12, 2011
 --
 -- REVISIONS: (Date and Description)
 --
 -- DESIGNER: Aman Abdulla
 --
 -- PROGRAMMER: Luke Queenan
 --
 -- INTERFACE: static void systemFatal(const char* message);
 --
 -- RETURNS: void
 --
 -- NOTES:
 -- This function displays an error message and shuts down the program.
 */
static void systemFatal(const char* message)
{
    perror(message);
    exit(EXIT_FAILURE);
}
