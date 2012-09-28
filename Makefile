cc = gcc
CFLAGS = -std=gnu99 -pedantic -Wall -Werror
RELEASE_FLAGS = -O2
SERVER_FILES = AddressUtility.c DieWithMessage.c TCPEchoServer4.c TCPServerUtility.c napster.c
CLIENT_FILES = TCPEchoClient4.c DieWithMessage.c TCPServerUtility.c AddressUtility.c napster.c

server : $(SERVER_FILES)
	$(cc) $(CFLAGS) $(SERVER_FILES) -o EchoServer4

client : $(CLIENT_FILES)
	$(cc) $(CFLAGS) $(CLIENT_FILES) -o EchoClient4

#TCPServerUtility.o : TCPServerUtility.c Practical.h 
#	$(cc) $(CFLAGS) TCPServerUtility.c

#DieWithMessage.o : DieWithMessage.c
#	$(cc) $(CFLAGS) DieWithMessage.c

#TCPEchoServer4.o : TCPEchoServer4.c Practical.h
#	$(cc) $(CFLAGS) TCPEchoServer4.c

#TCPEchoClient4.o : TCPEchoClient4.c Practical.h
#	$(cc) $(CFLAGS) TCPEchoClient4.c

#AddressUtility.o : AddressUtility.c
#	$(cc) $(CFLAGS) AddressUtility.c
clean : 
	rm EchoClient4 EchoServer4
