#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<sys/stat.h>
#define MAX 100


int main()
{
	int sockfd,fd,clilen,bytes=0,words=0,i,signal,curr,passes=0;
	char file_name[MAX],buffer[MAX];
	struct sockaddr_in servaddr;
	sockfd = (socket(AF_INET,SOCK_STREAM,0));

	if(sockfd<0)
	{
		perror("Unable to create socket\n");
		exit(0);
	}

	servaddr.sin_addr.s_addr = INADDR_ANY; // server address is local host
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(8181);

	//establishing a connection to the server
	signal = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if(signal<0)
	{
		perror("Unable to connect to server\n");
		exit(0);
	}

	printf("Enter file name...\n");
	scanf("%s",file_name);
	printf("Sending the name of the file to the server..\n");
	send(sockfd,file_name,strlen(file_name)+1,0);


	fd = open("Client_side.txt",O_WRONLY|O_CREAT|O_TRUNC);
	if(fd<0)
	{
		perror("Error creating file on the client side\n");
		exit(0);
	}

	while(curr = recv(sockfd,buffer,MAX,0))
	{
		passes++;

		//Copying server message to file
		for(i=0;i<curr;i++)
		{
			if(buffer[i]!='\0')
			{
				write(fd,&buffer[i],1);
				bytes++;
			}
		}
		int flag = 0;
		char pre = ' ';
		int ok = 0;
		//counting number of words
		for(i=0;i<curr;i++)
		{
			if(buffer[i]==',' || buffer[i]=='.' || buffer[i]==';' || buffer[i]==':' || buffer[i]==' ' || buffer[i]=='\t' || buffer[i]=='\n' || buffer[i]=='\0')
			{
				flag = 1;
			}
			if(pre==',' || pre=='.' || pre==';' || pre==':' || pre==' ' || pre=='\t' || pre=='\n' || pre=='\0')
			{
				if(flag == 0)
				{
					ok = 1;
				}
			}
			if(ok) words++;
			flag = 0;
			ok = 0;
			pre = buffer[i];
		}
	}

	if(passes==0 && bytes==0)
	{
		close(fd);
		printf("File not found\n");
		remove("Client_side.txt");
	}
	else
	{
		close(fd);
		printf("The file transfer is succesful\n");
		printf("Size of the file : %d\n",bytes);
		printf("Number of words in the file : %d\n",words);
	}

	close(sockfd);
	exit(0);
	return 0;
}