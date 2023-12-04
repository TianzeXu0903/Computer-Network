/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <cstddef>

using namespace std;

#define PORT "3490" // the port client will be connecting to

#define MAXDATASIZE 1024 // max number of bytes we can get at once


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
    std::string addr, protocol, host, port = "80", path;

	if (argc != 2) {
	    fprintf(stderr,"Usage: c./http_client http://hostname[:port]/path/to/file\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	std::string url(argv[1]);
	size_t pos = url.find("://");
	if (pos == std::string::npos) {
        fprintf(stderr, "Invalid URL");
        exit(1);
	}

	protocol = url.substr(0, pos);

	std::string remaining = url.substr(pos + 3);

	pos = remaining.find(":");
	if (pos != std::string::npos) {
        host = remaining.substr(0, pos);
        remaining = remaining.substr(pos + 1);
        size_t pos_slash = remaining.find("/");
        if (pos_slash != std::string::npos) {
            port = remaining.substr(0, pos_slash);
            path = remaining.substr(pos_slash + 1);
        }
        else {
            port = remaining;
        }
	} else {
        size_t pos_slash = remaining.find("/");
        if (pos_slash != std::string::npos) {
            host = remaining.substr(0, pos_slash);
            path = remaining.substr(pos_slash + 1);
        } else {
            port = remaining;
        }
	}

	cout << "prot:" << protocol << endl;
	cout << "host:" << host << endl;
	cout << "port:" << port << endl;
	cout << "path:" << path << endl;


	if (path.empty()) {
        fprintf(stderr, "Path Not Specified");
        exit(1);
	}

	if ((rv = getaddrinfo(host.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure


	std::string request = "GET " + path + " HTTP/1.1\r\n" + "User-Agent: Wget/1.12(linux-gnu)\r\n" + "Host: " + host + ":" + port + "\r\n" + "Connection: Keep-Alive\r\n\r\n";
	send(sockfd, request.c_str(), request.size(), 0);

	ofstream output_file("output", ios::binary);
	size_t tmppos;
	int loop_index = 0;
	while (true) {
        memset(buf, '\0', MAXDATASIZE);
        numbytes = recv(sockfd, buf, MAXDATASIZE, 0);
        cout << "loop" << loop_index << "starts" << endl;
        cout << "content: " << (std::string)buf << endl;
        if (numbytes > 0) {
            if ((tmppos = ((std::string)buf).find("\r\n\r\n")) != std::string::npos) {
                cout << "solve header" << endl;
                char* head = strstr(buf, "\r\n\r\n") + 4;
                output_file.write(head, strlen(head));
            } else {
                cout << "receiver is writing content..." << endl;
                output_file.write(buf, numbytes);}
        } else {
            break;
        }
        cout << "loop" << loop_index << "ends" <<endl;
        loop_index ++;
	}

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}
	// buf[numbytes] = '\0';

	// printf("client: received '%s'\n",buf);
    output_file.close();
	close(sockfd);

	return 0;
}
