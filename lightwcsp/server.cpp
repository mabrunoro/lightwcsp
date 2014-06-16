#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>

#include <iostream>
#include <string>
#include <cstring>
#include <regex>
#include <thread>

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"

#define PORT "80"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

#define BUFF_SIZE 1024
#define URL_SIZE 512

using namespace std;

#if 0
// not implemented yet, get busy people!
Logger logger1;
Config config; // create a config object
#endif

//namespace regExpressions {
  //const regex digits("[0-9]+");
  //const regex method("(GET|POST)\\s(\\S*)\\s(\\S*)");
  //const regex urlPath("^([\\S]*)\\?(\\S*)$");
  //const regex pathEnd("\\/$");
//}

void accept_request(int client);
void not_found(int client);
void serve_file(int client, const char *filename);
int startup(u_short *port);
void unimplemented(int client);
void headers(int client, const char *filename);
void cat(int client, FILE *resource);
void error_die(const char *sc);

void inline sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void inline *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void accept_request(int client)
{
	char buf[BUFF_SIZE];
	int numchars;
	size_t i, j;
	struct stat st;
	int cgi = 0;      /* becomes true if server decides this is a CGI
	 * program */

	numchars = recv(client, buf, BUFF_SIZE-1,0);

	smatch sm;
	string received(buf);
	//if (regex_match(received, sm, regExpressions::method))
	//{
	//	if(sm[1].compare("GET") == 0) { //GET

	//	}
	//	else if(sm[1].compare("POST") == 0) {
	//		cgi = 1;
	//	}
	//}
	if(received.compare(0,3,"GET")==0){
	  cout<<"Received:" <<received<<"\n";

	}
	else if(received.compare(0,4,"POST") == 0) {
	     cgi = 1;
	}
	else {
		unimplemented(client);
		return;
	}

	smatch urlMatch;
	string url;
	url = sm[2];
	/*if(regex_match(url, urlMatch, regExpressions::urlPath)) {
		url = urlMatch[1];
		cgi = 1;
		}*/

	//    sprintf(path, "htdocs%s", url);
	string path = "htdocs" + url;
	if (path[path.size()-1]=='/') {
		path+= "index.html";
	}
	if (stat(path.c_str(), &st) == -1) {
		while ((numchars > 0)/*&& strcmp("\n", buf)*/)  /* read & discard headers */
			numchars = recv(client, buf, BUFF_SIZE-1,0);
		not_found(client);
	}
	else
	{
		if ((st.st_mode & S_IFMT) == S_IFDIR)
			path+= "index.html";
		if ((st.st_mode & S_IXUSR) ||
				(st.st_mode & S_IXGRP) ||
				(st.st_mode & S_IXOTH)    )
			cgi = 1;
		if (!cgi)
			serve_file(client, path.c_str());
		//        else
		//            execute_cgi(client, path, method, query_string);
	}
	close(client);
}

void not_found(int client)
{
	char buf[BUFF_SIZE];
	snprintf(buf, BUFF_SIZE, strcat(strcat("HTTP/1.0 404 NOT FOUND\r\n",SERVER_STRING),
			"Content-Type: text/html\r\n\r\n<HTML><TITLE>Not Found</TITLE>\r\n\
	    		<BODY><P>The server could not fulfill\r\nyour request because the resource specified\r\n\
				is unavailable or nonexistent.\r\n</BODY></HTML>\r\n"));
	send(client, buf, strlen(buf), 0);
}

void serve_file(int client, const char *filename)
{
	FILE *resource = NULL;
	int numchars = 1;
	char buf[BUFF_SIZE];

	buf[0] = 'A'; buf[1] = '\0';
	//    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
	//    	numchars = recv(client, buf, BUFF_SIZE-1,0);

	resource = fopen(filename, "r");
	if (resource == NULL)
		not_found(client);
	else
	{
		headers(client, filename);
		cat(client, resource);
	}
	fclose(resource);
}

void headers(int client, const char *filename)
{
	char buf[BUFF_SIZE];
	(void)filename;  /* could use filename to determine file type */

	strcpy(buf, strcat(strcat("HTTP/1.0 200 OK\r\n",SERVER_STRING),"Content-Type: text/html\r\n\r\n"));
	send(client, buf, strlen(buf), 0);
}

void cat(int client, FILE *resource)
{
	char buf[BUFF_SIZE];
	while (fgets(buf, BUFF_SIZE, resource) != NULL)
	{
		send(client, buf, strlen(buf), 0);
	}
}

int startup(u_short *port)
{
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // Make connection IP version agnostic
	hints.ai_socktype = SOCK_STREAM;  // Will use Stream sockets and therefore, TCP
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		//		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		//		return 1;
		error_die("getaddrinfo");
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
		perror("Server failed to bind\n");
		exit(1);
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

	*port = atoi(PORT);
	return sockfd;
}

void error_die(const char *sc)
{
	perror(sc);
	exit(1);
}

void unimplemented(int client)
{
	char buf[BUFF_SIZE];
	snprintf(buf, BUFF_SIZE, strcat(strcat("HTTP/1.0 501 Method Not Implemented\r\n",SERVER_STRING),
			"Content-Type: text/html\r\n\r\n<HTML><HEAD><TITLE>Method Not Implemented\r\n</TITLE>\
    		</HEAD>\r\n<BODY><P>HTTP request method not supported.\r\n</BODY></HTML>\r\n"));
	send(client, buf, strlen(buf), 0);
}

int main(void)
{
	int server_sock = -1;
	u_short port = 0;
	int client_sock = -1;
	struct sockaddr_in client_name;
	int client_name_len = sizeof(client_name);
	//pthread_t newthread;

	server_sock = startup(&port);
	printf("httpd running on port %d\n", port);

	while (1)
	{
		client_sock = accept(server_sock,
				(struct sockaddr *)&client_name,
				&client_name_len);
		if (client_sock == -1)
			error_die("accept");
		/* accept_request(client_sock); */
		thread newThread(accept_request,client_sock);
	}

	//	while(1) {  // main accept() loop
	//		sin_size = sizeof their_addr;
	//		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
	//		if (new_fd == -1) {
	//			perror("accept");
	//			continue;
	//		}
	//
	//		inet_ntop(their_addr.ss_family,
	//				get_in_addr((struct sockaddr *)&their_addr),
	//				s, sizeof s);
	//		printf("server: got connection from %s\n", s);
	//
	//		if (!fork()) { // this is the child process
	//			close(sockfd); // child doesn't need the listener
	//			char message[] = "Hello, world! and a longer message to test!!!";
	//			if (send(new_fd, message, sizeof(message), 0) == -1)
	//				perror("send");
	//			close(new_fd);
	//			exit(0);
	//		}
	//		close(new_fd);  // parent doesn't need this
	//	}
	close(server_sock);
	return(0);
}

