#include<stdio.h>
#include<string.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<stdlib.h>
#include <unistd.h>

#define MAX 100

int main()
{
	int sockfd,newfd,clilen,i,fd,curr,signal;
	char buffer[MAX],filename[MAX];
	struct sockaddr_in cliaddr,servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM,0);

	if(sockfd < 0)
	{
		perror("Unable to create socket\n");
		exit(0);
	}

	servaddr.sin_port = htons(8181);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr  = INADDR_ANY;

	signal = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

	if(signal<0)
	{
		perror("Unable to bind\n");
		exit(0);
	}


	listen(sockfd,3);

	printf("Server running..\n");
	
	clilen = sizeof(cliaddr);
	newfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
	if(newfd<0)
	{
		perror("Error encountered while accepting\n");
		exit(0);
	}

	int j = 0,flag = 0;
	while(curr = recv(newfd,buffer,MAX,0))
	{
		for(i=0;i<curr;i++)
		{
			filename[j] = buffer[i];
			j++;
			if(buffer[i]=='\0')
			{
				flag = 1;
				break;
			}
		}
		if(flag==1)
			break;
	}
	filename[j] = '\0';
	fd = open(filename,O_RDONLY);
	if(fd<0)
	{
		perror("Error openeing file\n");
		close(newfd);
		close(sockfd);
		exit(0);
	}
	else
	{
		printf("File : %s\n",filename);
		int curr=0;
		do
		{
		    curr = read(fd,buffer,MAX-1);
		    buffer[curr]='\0';
		    send(newfd,buffer,strlen(buffer)+1,0);
		}	while(curr==MAX-1);
		close(newfd);
	}
	close(sockfd);
	return 0; 
}