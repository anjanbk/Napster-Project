#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Practical.h"
#include "state.h"

#define DEBUG 1
#define MAX_STRING_SIZE 50
#define HELP_STR "addfile - Add a file to the Napster server for sharing (usage: addfile <filename>)\naddfile -d - Stop sharing a file by having the server un-index it. (usage:addfile -d <file to be deleted>)\nfilelist - Display all files that are available for download\nhelp - Print this dialog\nquit - Exit Napster client\n"


// Payload elements: This information will be sent to the server in the form of a string
int STATE = SHELL;
char* file;

/**
 * Parses the user's input. If valid, it will return a state, which will be sent to the server.
 */
void parse(char* input)
{
	// Split the string into all of its tokens
	char* token;
	char* delimiter = " ";
	token = strtok(input, delimiter);
	while (token != NULL)	
	{
		if (DEBUG) printf("Entered parse while loop\n");
		if (STATE == ADDFILE)
		{
			if (DEBUG) printf("Currently in addfile state\n");
			if (strcmp(token,"-d") == 0)
				STATE = DELETEFILE;
			else
				file = token;
		}
		else if (STATE == DELETEFILE)
			file = token;
		else
		{	
			if (strcmp(token, "quit") == 0)	
			{
				if (DEBUG) printf("strcmp(token, quit) is true\n");
				STATE = QUIT;	
			}
			else if (strcmp(token,"addfile") == 0)
			{
				STATE = ADDFILE;
			}
			else if (strcmp(token, "help") == 0)
			{
				if (DEBUG) printf("strcmp(token, help) is true\n");
				STATE = HELP;
			}
			else if (strcmp(token, "filelist") == 0)
			{
				if (DEBUG) printf("strcmp(token, filelist) is true\n");
				STATE = LISTFILES; 
			}
			else
			{
				STATE = INVALID;
			}
		}
		token = strtok(NULL, delimiter);
	} 
	if (DEBUG) printf("State is: %d", STATE);
}

int main(int argc, char *argv[]) {
  if (argc < 3 || argc > 4) // Test for correct number of arguments
    DieWithUserMessage("Parameter(s)",
        "<Server Address> <Echo Word> [<Server Port>]");
  char *servIP = argv[1];     // First arg: server IP address (dotted quad)
  char *echoString = argv[2]; // Second arg: string to echo

  // Third arg (optional): server port (numeric).  7 is well-known echo port
  in_port_t servPort = (argc == 4) ? atoi(argv[3]) : 7;

  // Create a reliable, stream socket using TCP
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0)
    DieWithSystemMessage("socket() failed");

  // Construct the server address structure
  struct sockaddr_in servAddr;            // Server address
  memset(&servAddr, 0, sizeof(servAddr)); // Zero out structure
  servAddr.sin_family = AF_INET;          // IPv4 address family
  // Convert address
  int rtnVal = inet_pton(AF_INET, servIP, &servAddr.sin_addr.s_addr);
  if (rtnVal == 0)
    DieWithUserMessage("inet_pton() failed", "invalid address string");
  else if (rtnVal < 0)
    DieWithSystemMessage("inet_pton() failed");
  servAddr.sin_port = htons(servPort);    // Server port

  // Establish the connection to the echo server
	if (connect(sock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
		DieWithSystemMessage("connect() failed");

	size_t echoStringLen = strlen(echoString); // Determine input length

	printf("Napster Client. Type \"help\" at any time for more options.\n");
	// Begin interactive Napster shell
	for (;;)
	{
		printf("napster>> ");
		char input[MAX_STRING_SIZE] = {0};
		char payloadString[MAX_STRING_SIZE] = {0};
		char stateStr[1];
		gets(input);
		
		parse(input);
		sprintf(stateStr, "%d", STATE);
		strcat(payloadString, stateStr);
		if (DEBUG) printf("Payload String: %s\n",payloadString);
		switch (STATE)
		{
			case ADDFILE:	
			case DELETEFILE:	strcat(payloadString, file);
						break;
			case HELP:		printf(HELP_STR);
				  		break;
			case QUIT:		close(sock);
						exit(0);
			case INVALID:		printf("Command not found.\n");
						continue;
		}
		echoStringLen = strlen(payloadString);
		if (DEBUG) printf("Length of the payload is: %d\n", (int)echoStringLen);

		// Send the string to the server to handle	
		ssize_t numBytes = send(sock, payloadString, strlen(payloadString), 0); 
		if (numBytes < 0)
			DieWithSystemMessage("send() failed");
		else if (numBytes != strlen(payloadString))
			DieWithUserMessage("Send()", "sent unexpected number of bytes");
	
		// Receive an echo from the server
		unsigned int totalBytesRcvd = 0; // Count of total bytes received
		while (totalBytesRcvd < echoStringLen) {
			char buffer[BUFSIZE]; // I/O buffer
			/* Receive up to the buffer size (minus 1 to leave space for
			a null terminator) bytes from the sender */
			if (DEBUG) printf("Listening for echo from the server...\n");
			numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
			if (numBytes < 0)
				DieWithSystemMessage("recv() failed");
			else if (numBytes == 0)
				DieWithUserMessage("recv()", "connection closed prematurely");
			totalBytesRcvd += numBytes; // Keep tally of total bytes
			buffer[numBytes] = '\0';    // Terminate the string!
			if (DEBUG) printf("Heard echo from server: %s, proceeding...\n",buffer);
		}

		if (STATE == LISTFILES)
		{
			/*
				Here's where things get a little interesting.
				When we do "filelist", the sheer size of the list
				of files on the central server can very well be 
				larger than our MAX_BUFFER_SIZE (which we have specified).
				Therefore, here's what we do:
					1) Send the "filelist" command to the server (which we've done above)
					2) The server will fetch the filelist string, calculate its length and send it back
					3) We (the client) will use this value to initialize our character buffer (allocating the appropriate amount of memory)
					4) We send an acknowledgement to the server
					5) Finally, it will send us the filelist string, where we will have the proper buffer to receive it.
			*/
			// Receive size
			totalBytesRcvd = 0;
			int fileListLength = 0;
			char buffer[BUFSIZE];
			while (totalBytesRcvd == 0)
			{
				if (DEBUG) printf("Waiting for server to send size...\n");
				numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
				if (numBytes < 0)
					DieWithSystemMessage("recv() failed");
				else if (numBytes == 0)
					DieWithUserMessage("recv()", "connection closed prematurely");
				totalBytesRcvd += numBytes; // Keep tally of total bytes
				buffer[numBytes] = '\0';    // Terminate the string!
				if (DEBUG) printf("Size of file list received from server...\n");
			}
			fileListLength = atoi(buffer);	
			char fileList[fileListLength]; 
		
			// Send acknowledgment
			numBytes = send(sock, "ACK", 3, 0); 
			if (numBytes < 0)
				DieWithSystemMessage("send() failed");
			else if (numBytes != 3)
				DieWithUserMessage("Send()", "sent unexpected number of bytes");

			// Receive list
			totalBytesRcvd = 0;
			while (totalBytesRcvd == 0)
			{
				if (DEBUG) printf("Waiting for server to send list...\n");
				numBytes = recv(sock, fileList, fileListLength - 1, 0);
				if (numBytes < 0)
					DieWithSystemMessage("recv() failed");
				else if (numBytes == 0)
					DieWithUserMessage("recv()", "connection closed prematurely");
				totalBytesRcvd += numBytes; // Keep tally of total bytes
				fileList[numBytes] = '\0';    // Terminate the string!
				if (DEBUG) printf("List received...\n");
			}
			printf("File List:\n%s\n",fileList);
		}
		
		STATE = SHELL;
	}

  // Send the string to the server TODO:CHANGE
	
  ssize_t numBytes = send(sock, echoString, echoStringLen, 0);
  if (numBytes < 0)
    DieWithSystemMessage("send() failed");
  else if (numBytes != echoStringLen)
    DieWithUserMessage("send()", "sent unexpected number of bytes");

  // Receive the same string back from the server
  unsigned int totalBytesRcvd = 0; // Count of total bytes received
  fputs("Received: ", stdout);     // Setup to print the echoed string
  while (totalBytesRcvd < echoStringLen) {
    char buffer[BUFSIZE]; // I/O buffer
    /* Receive up to the buffer size (minus 1 to leave space for
     a null terminator) bytes from the sender */
    numBytes = recv(sock, buffer, BUFSIZE - 1, 0);
    if (numBytes < 0)
      DieWithSystemMessage("recv() failed");
    else if (numBytes == 0)
      DieWithUserMessage("recv()", "connection closed prematurely");
    totalBytesRcvd += numBytes; // Keep tally of total bytes
    buffer[numBytes] = '\0';    // Terminate the string!
    fputs(buffer, stdout);      // Print the echo buffer
  }

  fputc('\n', stdout); // Print a final linefeed

  close(sock);
  exit(0);
}
