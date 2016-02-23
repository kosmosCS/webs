// A simple program that computes the square root of a number
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <iostream>
#include "errno.h"
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <set>

#include <algorithm>



#define CLIENT_TTL 3600
    //"Content-Length: 122\r\n"

#define HTTP_200_HEADER_HTML \
	"HTTP/1.1 200 OK\r\n" \
    "Content-Type: text/html\r\n" \
    "\r\n"

#define HTTP_200_HEADER_PLAIN \
	"HTTP/1.1 200 OK\r\n" \
    "Content-Type: text/plain\r\n" \
    "\r\n"

// static const char* not_found_response_valid_command =
// 	"HTTP/1.0 404 Not Valid Command\r\n"
// 	"Content-type: text/html\r\n\r\n"
// 	"<html>\n"
// 	"<body>\n"
// 	"<h1>Not Valid Command</h1>\n"
// 	"<p>The requested URL is not valid Command.</p>\n"
// 	"</body>\n"
// 	"</html>\n";

	static const char* not_found_response_template =
	"HTTP/1.0 404 Not Found\r\n"
	"Content-type: text/html\r\n\r\n"
	"<html>\n"
	"<body>\n"
	"<h1>Not Found</h1>\n"
	"<p>The requested URL was not found on the server.</p>\n"
	"</body>\n"
	"</html>\n";


static const char* not_found_response_valid_command_raw =
	"HTTP/1.0 404 Not Valid Command\r\n"
	"Content-type: text/plain\n\n"
	"Not Valid Command\n";

const int default_permission = S_IRUSR | S_IWUSR | 
                         S_IRGRP | S_IWGRP |
                         S_IROTH | S_IWOTH;

static char PATH[200];


int set_non_block(int fd) {
	int flags;
#if defined(O_NONBLOCK)
	if(-1 ==(flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}


void listenSocket(int MasterSocket);

int getInputs(int argc, char *argv[], char *ip, int *port, char *dir);

int setupSocket(char *ip, int port);

void http_handler(int SlaveSocket, char *Buf);


void sighandler(int signum) {
    waitpid(0, 0, WNOHANG);
}

int main (int argc, char *argv[])
{
	char ip_address[20], directory[200];
	int port;
	struct sigaction sa;
	
	if(getInputs(argc, argv, &ip_address[0], &port, &directory[0]) != 0) {
		exit(EXIT_FAILURE);
	}

	strcpy(PATH, directory);

	//printf("ip - %s, port - %d, directory - %s\n", ip_address, port, directory);

	int myfd;

	char str[15];
	sprintf(str, "%d", port);

	myfd = open("log.txt", O_CREAT | O_WRONLY, default_permission);

	write(myfd, ip_address,	strlen(ip_address));
	write(myfd, "\n", 1);
	write(myfd, str, strlen(str));
	write(myfd, "\n",1);
	write(myfd, directory, strlen(directory));
	close(myfd);

	int pid = fork();

	switch(pid) {
		case 0:
			setsid();
		    chdir(directory);
		    chroot(directory);

		    close(0);
		    close(1);
		    close(2);

		    memset(&sa, 0, sizeof(sa));
		    sa.sa_handler = &sighandler;
		    sigaction(SIGCHLD, &sa, 0);

		    int MasterSocket;
			MasterSocket = setupSocket(ip_address, port);

			set_non_block(MasterSocket);


			listenSocket(MasterSocket);
			exit(1);
		    exit(0);
		case -1:
	    //	std::cout << "fork() error" << std::endl;
	    break;

		default:
	    	//std::cout << "ok. PID=" << pid << std::endl;
		    break;

	}

	exit(EXIT_SUCCESS);
	
  return 0;
}


int setupSocket(char *ip, int port) {

	int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int enable = 1;
	if (setsockopt(MasterSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed");

	if (MasterSocket == -1) {
		perror("Master error: ");
		exit(1);
	}

	struct sockaddr_in SockAddr;
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_port = htons(port);
	SockAddr.sin_addr.s_addr = inet_addr(ip);

	if((bind(MasterSocket, (struct sockaddr *) (&SockAddr), sizeof(SockAddr))) != 0) {
		perror("Bind error: ");
		exit(1);
	} 


	return MasterSocket;
}

int getInputs(int argc, char *argv[], char *ip, int *port, char *dir) {
	int opt;

	while ((opt = getopt(argc, argv, "h:p:d:")) != -1) {
		switch (opt) {
			case 'h':
				strcpy(ip,optarg);
				break;
			case 'p':
				*port = atoi(optarg);
				break;
			case 'd':
				strcpy(dir, optarg);
				break;
			default: /* '?' */
				fprintf(stderr, "Usage: %s [-t nsecs] [-n] name\n",
				argv[0]);
				return EXIT_FAILURE;
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Expected argument after options\n");
		return EXIT_FAILURE;
	}
	return 0;

}


void listenSocket(int MasterSocket) {
	std::set<int> SlaveSockets;

	listen(MasterSocket, SOMAXCONN);
	while(1) {
		fd_set Set;
		FD_ZERO(&Set);
		FD_SET(MasterSocket, &Set);


		for(auto Iter = SlaveSockets.begin();
					Iter != SlaveSockets.end(); Iter++) {
				FD_SET(*Iter, &Set);
		}

		int Max = std::max(MasterSocket,
				*std::max_element(SlaveSockets.begin(),
					SlaveSockets.end()));

		select(Max + 1, &Set, NULL, NULL, NULL);
		

		for (auto Iter = SlaveSockets.begin();
					Iter != SlaveSockets.end(); Iter++) {
			if(FD_ISSET(*Iter, &Set)) {
				//code handler to receive and send
				static char Buffer[1024];
				int RecvSize = recv(*Iter, Buffer, 1024, MSG_NOSIGNAL);
				if((RecvSize == 0) && (errno != EAGAIN)) {
					shutdown(*Iter, SHUT_RDWR);
					close(*Iter);
					SlaveSockets.erase(Iter);
				} else if (RecvSize != 0){
					http_handler(*Iter, Buffer);
				//	printf("user disconnected - %d\n",*Iter);
					shutdown(*Iter, SHUT_RDWR);
					close(*Iter);
					SlaveSockets.erase(Iter);
				}
				
			}
		}

		if(FD_ISSET(MasterSocket, &Set)) {
			int SlaveSocket = accept(MasterSocket, 0, 0);
			set_non_block(SlaveSocket);
			SlaveSockets.insert(SlaveSocket);
		}
		

		
	}
}

void http_handler(int SlaveSocket, char *Buf) {
	char method[10];
	char url[500];
	char protocol[500];

	sscanf (Buf, "%s %s %s", method, url, protocol);

	if(!(strcmp(method, "GET"))) {
		//if(!strcmp(url,"/")) {
			
			
		//} else {
			int f, i;

			for (i = 1; i < strlen(url); i++) {
				url[i - 1] = url[i];
			}
			url[i - 1] = '\0';

			for (i = 0; i < strlen(url); i++) {
				if(url[i] == '?') {
					url[i] = '\0';
					break;
				}
			}


			f = open(url, O_RDONLY, default_permission);

			if(f != -1) {
				memset(Buf, '\0', strlen(Buf));
				int nbytes = read(f, Buf, 50000);

				if (nbytes > 0) {
					send(SlaveSocket, HTTP_200_HEADER_HTML, strlen(HTTP_200_HEADER_HTML), MSG_MORE);
					/*if(strstr(url, ".html") == 0 || strstr(url, ".htm") != 0) {
						send(SlaveSocket, HTTP_200_HEADER_HTML, strlen(HTTP_200_HEADER_HTML), MSG_MORE);
					}
					else {
						send(SlaveSocket, HTTP_200_HEADER_PLAIN, strlen(HTTP_200_HEADER_PLAIN), MSG_MORE);
					} */
					send(SlaveSocket, Buf, strlen(Buf), MSG_DONTWAIT);
				} else {
					send(SlaveSocket, not_found_response_template,
					strlen(not_found_response_template), MSG_DONTWAIT);
				}
				
			} else {
				send(SlaveSocket, not_found_response_template,
					strlen(not_found_response_template), MSG_DONTWAIT);
			}
			close(f);
		//}
	} else {
		send(SlaveSocket, not_found_response_template,
			strlen(not_found_response_template), MSG_DONTWAIT);
	}
}