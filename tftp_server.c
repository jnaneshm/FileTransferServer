/*
** tftp_server.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define MYPORT "5000"	// the port users will be connecting to
#define MAXBUFLEN 100

	char buf[MAXBUFLEN];
	char pkt[516];
	struct sockaddr_storage their_addr;

struct tftp {
	uint16_t opcode;

};

void decode_rrq(char* bf,char* file,char* mode){
	//char file[20];
	//char mode[10];
	char zero;
	uint16_t network,host;
	int k,i;
	//bf+=2;
	printf("\nDecoding RRQ...");
	for(i=0;1;i++){	
		//printf("bf:%c\n",bf[i]);
		memcpy(file+i,bf+i,1);
		memcpy(&zero,bf+i+1,1);
		//printf("zero=%d\n",zero);
		if(!zero){ 
			//printf("\nEND\n"); 
			break;
		}
	}
	file[++i]='\0';
	//printf("\nfilename=[%s]",file);

        for(i++,k=0;1;k++,i++){
                //printf("bf:%c\n",bf[i]);
                memcpy(mode+k,bf+i,1);
                memcpy(&zero,bf+i+1,1);
                //printf("zero=%d\n",zero);
                if(!zero){
                        //printf("\nEND\n");
                        break;
                }
        }
        mode[++k]='\0';
        //printf("\nmode=[%s]",mode);

	return;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
	return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
	struct stat file_stat;
	FILE *fp;
	int to_send,blk_ack=0,blk_num,sent=0;
	uint16_t datacode,block;
	int sockfd,fdmax,root,opcode;
	struct addrinfo hints, *servinfo, *p;
	int rv,c;
	int numbytes,sentbytes,recvbytes;
	char* tmp;
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];
	char file[20], mode[10];	
	uint16_t network,host;
	
	fd_set tree,reads;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // set to AF_INET to force IPv4

	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE; // use my IP
	if ((rv = getaddrinfo(NULL, MYPORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
		perror("tftp_server: socket");
		continue;
		}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
		close(sockfd);
		perror("tftp_server: bind");
		continue;
		}
		break;
	}
	if (p == NULL) {
		fprintf(stderr, "listener: failed to bind socket\n");
		return 2;
	}
	freeaddrinfo(servinfo);
	FD_SET(sockfd,&tree);
	fdmax = sockfd;	
	printf("listener: waiting to recvfrom...\n");

	for(;;){
		reads = tree;
		if(select(fdmax+1, &reads, NULL,NULL,NULL)==-1){
			perror("select");
			exit(EXIT_FAILURE);
		}

			addr_len = sizeof their_addr;
			if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 ,0,(struct sockaddr *)&their_addr, &addr_len))==-1) 						{
				perror("recvfrom");
				exit(1);
			}
			printf("listener: got packet from %s\n",
			inet_ntop(their_addr.ss_family,	get_in_addr((struct sockaddr *)&their_addr),s, sizeof s));
			printf("listener: packet is %d bytes long\n", numbytes);
			buf[numbytes] = '\0';
			printf("listener: packet contains \"%s\"\n", buf);
			
			//Check opcode
			tmp=buf;
			memcpy((char *)&network,tmp,2);
			opcode=ntohs(network);
			printf("Opcode=%d\n",opcode);				
			tmp+=2;	
			switch(opcode){
				case 1 : // RRQ
					decode_rrq(tmp,file,mode);
					printf("\nRRQ:file=%s,mode=%s\n",file,mode);	
					fp=fopen(file,"r");
					if(fp==NULL){
						printf("ERR:File cant be opened\n"); exit;	
					}
					printf("File opened successfully\n");
					if ( stat(file, &file_stat) == -1 )
						printf("File size ERROR\n");	
					printf("Size of file \"%s\" is %ld bytes\n", file, file_stat.st_size);
					blk_num=1;

					while(file_stat.st_size>sent){

					datacode=htons(3);
					memcpy(pkt,(char *)&datacode,2);
					block=htons(blk_num);
					
					memcpy(pkt+2,(char *)&block,2);
					if(file_stat.st_size<512) to_send=file_stat.st_size;
					else to_send=512;
					fread(pkt+4,1,to_send,fp);	
					printf("listener: sending packet to %s\n",
					inet_ntop(their_addr.ss_family,	get_in_addr((struct sockaddr *)&their_addr),s, sizeof s));
					if ((sentbytes = sendto(sockfd, pkt, to_send+4, 0, (struct sockaddr *)&their_addr, addr_len )) == -1) {
									perror("talker: sendto");
									exit(1);
					}
					printf("Sent block %d of DATA with %d bytes\n",blk_num,sentbytes);

					if ((recvbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 ,0,(struct sockaddr *)&their_addr, &addr_len))==-1) 						{
						perror("recvfrom");
						exit(1);
					}
					printf("tftp_server: got packet from %s\n",
					inet_ntop(their_addr.ss_family,	get_in_addr((struct sockaddr *)&their_addr),s, sizeof s));
					tmp=buf;
					memcpy((char *)&network,tmp,2);
					opcode=ntohs(network);
					printf("Opcode=%d\n",opcode);				
					tmp+=2;	
					if(opcode==4){
						memcpy((char *)&network,tmp,2);
						blk_ack=ntohs(network);
						printf("Recvd ACK for Blk=%d\n",blk_ack);
						if(blk_ack==blk_num){
							blk_num++;				
							sent+=sentbytes;
						}
					}
					else{printf("\nFile Transfer failed !!!\n");break;}
					//printf("\nFile Size is %ld but sent %d\n",file_stat.st_size,sent);
					} // end of while
					printf("\nFile Size is %ld but sent %d\n",file_stat.st_size,sent);
					if(file_stat.st_size<=sent)printf("\nFile Transfer completed !!!\n");
					fclose(fp);

					//cleanup
					memset(&their_addr,0,sizeof(their_addr));
					memset(&pkt,0,sizeof(pkt));
					sent=0;
					break;	
				case 2 : //WRQ
					printf("\nWRQ not supported\n");
					break;
				case 3 : //DATA 
					break;	
				case 4 : //ACK
					printf("Received ACK\n");
					break;	
					}	
	}
	
	return 0;
}
