#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "Practical.h"
#include "napster.h"
#include "state.h"

static const int MAXPENDING = 5; // Maximum outstanding connection requests
const char* FILE_LIST = "napster_server_files.list";

int SetupTCPServerSocket(const char *service) {
  // Construct the server address structure
  struct addrinfo addrCriteria;                   // Criteria for address match
  memset(&addrCriteria, 0, sizeof(addrCriteria)); // Zero out structure
  addrCriteria.ai_family = AF_UNSPEC;             // Any address family
  addrCriteria.ai_flags = AI_PASSIVE;             // Accept on any address/port
  addrCriteria.ai_socktype = SOCK_STREAM;         // Only stream sockets
  addrCriteria.ai_protocol = IPPROTO_TCP;         // Only TCP protocol

  struct addrinfo *servAddr; // List of server addresses
  int rtnVal = getaddrinfo(NULL, service, &addrCriteria, &servAddr);
  if (rtnVal != 0)
    DieWithUserMessage("getaddrinfo() failed", gai_strerror(rtnVal));

  int servSock = -1;
  for (struct addrinfo *addr = servAddr; addr != NULL; addr = addr->ai_next) {
    // Create a TCP socket
    servSock = socket(addr->ai_family, addr->ai_socktype,
        addr->ai_protocol);
    if (servSock < 0)
      continue;       // Socket creation failed; try next address

    // Bind to the local address and set socket to listen
    if ((bind(servSock, addr->ai_addr, addr->ai_addrlen) == 0) &&
        (listen(servSock, MAXPENDING) == 0)) {
      // Print local address of socket
      struct sockaddr_storage localAddr;
      socklen_t addrSize = sizeof(localAddr);
      if (getsockname(servSock, (struct sockaddr *) &localAddr, &addrSize) < 0)
        DieWithSystemMessage("getsockname() failed");
      fputs("Binding to ", stdout);
      PrintSocketAddress((struct sockaddr *) &localAddr, stdout);
      fputc('\n', stdout);
      break;       // Bind and listen successful
    }

    close(servSock);  // Close and try again
    servSock = -1;
  }

  // Free address list allocated by getaddrinfo()
  freeaddrinfo(servAddr);

  return servSock;
}


void viewFiles(int clntSock)
{
	FILE* fp = fopen(FILE_LIST, "rt");
	char line[80];
	char* list = malloc(1024 * sizeof(char)); 
	
	if (fp != NULL)
	{
		while (fgets(line, 1024, fp) != NULL)
		{
			strcat(list, line);
		}
		printf("Current line: %s\n", list);
	}
	char* buffer = "SIZE VALUE";

	// Send size
	ssize_t numBytesSent = send(clntSock, buffer, strlen(buffer), 0);
	if (numBytesSent < 0)
		DieWithSystemMessage("send() failed");
	else if (numBytesSent != strlen(buffer))
		DieWithUserMessage("send()", "sent unexpected number of bytes");

	// Receive Acknowledgement
	ssize_t numBytesRcvd = 0;
	char str[5];
	while (numBytesRcvd == 0)
	{
		printf("Waiting for acknowledgement from client (handshake)...\n");
		printf("Receiving acknowledgement\n");
		numBytesRcvd = recv(clntSock, str, 5, 0);
		if (numBytesRcvd < 0) {
			DieWithSystemMessage("recv() failed");
		}
		str[numBytesRcvd] = '\0';
		printf("Acknowledgement received...\n");
	}
	if (strcmp(str, "ACK") == 0)
	{
		// Send the file list
		FILE* fp = fopen(FILE_LIST, "rt");
		char line[80];
		char* list = malloc(1024 * sizeof(char)); 
		
		if (fp != NULL)
		{
			while (fgets(line, 1024, fp) != NULL)
			{
				strcat(list, line);
			}
		}

		if (strcmp(list,"") == 0)
			list = "Central server file list is empty!";

		numBytesSent = send(clntSock, list, strlen(list), 0);
		if (numBytesSent < 0)
			DieWithSystemMessage("send() failed");
		else if (numBytesSent != strlen(list))
			DieWithUserMessage("send()", "sent unexpected number of bytes");
	}
	else
		printf("Error listing files\n");
}

int addFile(char* filename, char* client)
{
	FILE* fp;
	fp = fopen(FILE_LIST, "a+");
	char* fileEntry = malloc((strlen(client)+strlen(filename)+2)*sizeof(char));
	strcat(fileEntry,filename);
	strcat(fileEntry," ");
	strcat(fileEntry,client);
	strcat(fileEntry,"\n");

	if (fp)
	{
		printf("File exists\n");
	}
	else
	{
		printf("File does not exist, creating...\n");
		fp = fopen(FILE_LIST, "w");
	}
	int success = fprintf(fp, "%s", fileEntry);
	fclose(fp);
	free(fileEntry);
	return success < 0 ? 0 : 1;
}

int deleteFile(char* filename, char* client)
{
	FILE* fp_old;
	FILE* fp_new;
	int fileFoundFlag = 0;
	fp_old = fopen(FILE_LIST, "r+");
	fp_new = fopen("temp.list", "w");
	char* fileEntry = malloc((strlen(client)+strlen(filename)+2)*sizeof(char));
	strcat(fileEntry,filename);
	strcat(fileEntry," ");
	strcat(fileEntry,client);
	strcat(fileEntry,"\n");

	// To delete, we will scan through the file and add every line to a new file, EXCEPT for the line which is to be deleted
	// We will then delete the old file and rename the new file
	
	if (fp_old != NULL)
	{
		// Copy lines
		char line[128];
		while (fgets(line, 128, fp_old) != NULL)
		{
			if (strcmp(line, fileEntry) != 0)
			{
				printf("Not deleting: %s\n",line);
				fprintf(fp_new, "%s", line);
			}
			else
			{
				printf("Deleting: %s\n", line);
				fileFoundFlag = 1;
			}
		}

		// Delete old file
		remove(FILE_LIST);
		
		// Rename new file
		rename("temp.list", FILE_LIST);
	}
	fclose(fp_old);
	fclose(fp_new);

	free(fileEntry);
	return fileFoundFlag;
}

int AcceptTCPConnection(int servSock) {
  struct sockaddr_storage clntAddr; // Client address
  // Set length of client address structure (in-out parameter)
  socklen_t clntAddrLen = sizeof(clntAddr);

  // Wait for a client to connect
  int clntSock = accept(servSock, (struct sockaddr *) &clntAddr, &clntAddrLen);
  if (clntSock < 0)
    DieWithSystemMessage("accept() failed");

  // clntSock is connected to a client!

  fputs("Handling client ", stdout);
  PrintSocketAddress((struct sockaddr *) &clntAddr, stdout);
  fputc('\n', stdout);

  return clntSock;
}

void HandleTCPClient(int clntSocket, char* clntName) {
	char buffer[BUFSIZE]; // Buffer for echo string

	// Receive message from client
	ssize_t numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
	if (numBytesRcvd < 0) {
		DieWithSystemMessage("recv() failed");
	}
	buffer[numBytesRcvd] = '\0';
	printf("Number of bytes received: %d\n", (int)numBytesRcvd);

  	// Send received string and receive again until end of stream
	while (numBytesRcvd > 0) { // 0 indicates end of stream
		// Echo message back to client
		ssize_t numBytesSent = send(clntSocket, buffer, numBytesRcvd, 0);
		if (numBytesSent < 0)
			DieWithSystemMessage("send() failed");
		else if (numBytesSent != numBytesRcvd)
			DieWithUserMessage("send()", "sent unexpected number of bytes");
		
		// Give the message to Napster's parser
		char* filename = malloc((numBytesRcvd-1)*sizeof(char));
		printf("Received string: %s\n", buffer);
		memmove(filename, &buffer[1], numBytesRcvd-1);
		filename[numBytesRcvd-1] = '\0';

		switch (buffer[0] - '0')
		{
			case ADDFILE: addFile(filename, clntName);
					break;
			case DELETEFILE: printf("Status of delete: %d\n",deleteFile(filename, clntName));
					break;
			case LISTFILES: viewFiles(clntSocket);
					break;
		}

		free(filename);

		// See if there is more data to receive
		printf("Waiting for more data from client...\n");
		numBytesRcvd = recv(clntSocket, buffer, BUFSIZE, 0);
		buffer[numBytesRcvd] = '\0';    // Terminate the string!
		if (numBytesRcvd < 0) {
			DieWithSystemMessage("recv() failed");
		}
		printf("Processing data recieved, \"%s\"...\n", buffer);
		printf("Number of bytes received (currently) : %d\n",(int)numBytesRcvd);
  	}

	close(clntSocket); // Close client socket
}
