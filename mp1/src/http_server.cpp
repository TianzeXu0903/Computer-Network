/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <istream>
#include <fstream>
#include <vector>

using namespace std;


#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

#define BUFFER_SIZE 1024

void sigchld_handler(int s)
{
    s -= 1;
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void handle_request(int socket) {

    char buffer[BUFFER_SIZE];
    int byte_received = recv(socket, buffer, sizeof(buffer), 0);

    if (byte_received <= 0) {
        return;
    }

    std::string request = buffer;

    cout << "request from client:" << request << endl;

    size_t pos = request.find("GET ");

    if (pos == std::string::npos) {
        std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(socket, response.c_str(), response.size(), 0);
        return;
    }

    std::string filepath = request.substr(pos + 4);
    filepath = filepath.substr(0, filepath.find(" "));
    if (!filepath.empty() && filepath[0] == '/') {
        filepath.erase(0, 1);
    }
    cout << "file path: " << filepath << endl;

    FILE *filep;
    char readbuf[BUFFER_SIZE];
    int numbytes;

    filep = fopen(filepath.data(), "rb");
    if (filep == NULL) {
        std::string response = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(socket, response.c_str(), response.size(), 0);
        return;
    } else {
        std::string response = "HTTP/1.1 200 OK\r\n\r\n";
        cout << "sending response" << endl;
        send(socket, response.c_str(), response.size(), 0);
    }


    memset(readbuf, '\0', BUFFER_SIZE);
    while (true) {
        cout << "server is sending content..." << endl;
        numbytes = fread(readbuf, sizeof(char), BUFFER_SIZE, filep);
        cout << "read content: " << readbuf << endl;
        if (numbytes > 0) {
            cout << "sent content: " << readbuf << endl;
            if (send(socket, readbuf, numbytes, 0) == -1) {
                perror("send");
                exit(1);
            }
            memset(readbuf, '\0', BUFFER_SIZE);
        }
        else {
            break;
        }
    }
    fclose(filep);
}

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	if (argc != 2) {
        fprintf(stderr, "Usage: ./http_server <port>\n");
        exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		if (!fork()) { // this is the child process
			close(sockfd); // child doesn't need the listener
            handle_request(new_fd);
			close(new_fd);
			exit(0);
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}


