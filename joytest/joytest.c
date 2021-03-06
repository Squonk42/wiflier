#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "os_generic.h"

int sockfd;
struct sockaddr_in servaddr,cliaddr;
int joyf;
int gotjoy = 0;
int joyd[4];

og_thread_t joy_t;
og_thread_t recv_t;

struct m_js_event {
	uint32_t time;     /* event timestamp in milliseconds */
	int16_t value;    /* value */
	uint8_t type;      /* event type */
	uint8_t number;    /* axis/button number */
};

void * joy( void * v )
{
	struct m_js_event e;
	int r;
	do
	{
		r =read (joyf, &e, sizeof(e));
		if( e.type != 2 ) continue;

		if( e.number < 4 )
		{
			joyd[e.number] = e.value;
			//printf( "%d / %d %d %d\n", e.time, e.value, e.type, e.number );
		}
		gotjoy = 0xffffff;

	} while( r == sizeof(e) );
	fprintf( stderr, "Error: Fault dealing with joystick\n" );
}

void * recvthd( void * v )
{
	FILE * thislog = fopen( "thislog.txt", "w" );
	char recvline[1000];
	int n;

	while(1)
	{
		n=recvfrom(sockfd,recvline,10000,0,NULL,NULL);
		recvline[n]=0;
		if( recvline[0] == 'T' && recvline[1] == 'D' )
		{
			fputs( recvline, thislog );
		}
		fputs(recvline,stdout);
	}
}

int main(int argc, char**argv)
{
	char sendline[1000];

	if (argc != 3)
	{
      fprintf( stderr, "usage:  udpcli <IP address> <joy file>\n");
      return -2;
	}

	sockfd=socket(AF_INET,SOCK_DGRAM,0);

	if( sockfd <= 0 )
	{
		fprintf( stderr, "Error: could not allocate socket.\n" );
		return -1;
	}

	joyf = open (argv[2], O_RDONLY);

	if( joyf <= 0 )
	{
		fprintf( stderr, "Error: could not open joystick.\n" );
		return -1;
	}

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=inet_addr(argv[1]);
	servaddr.sin_port=htons(7777);

	joy_t = OGCreateThread( &joy, 0 );
	recv_t = OGCreateThread( &recvthd, 0 );

	while(1)
	{
		int r = sendto(sockfd,"MA\n",3,0, (struct sockaddr *)&servaddr,sizeof(servaddr));
		if( r != 3 )
		{
			fprintf( stderr, "Error: could not send. %d\n", r  );
			exit( -4 );
		}

		r = sendto(sockfd,"T1\n",3,0, (struct sockaddr *)&servaddr,sizeof(servaddr));
		if( r != 3 )
		{
			fprintf( stderr, "Error: could not send. %d\n", r  );
			exit( -4 );
		}

		usleep(10000);

		if( ((gotjoy & 0x0f) == 0x0f ) )
		{
			int n = sprintf( sendline, "J%d:%d:%d:%d", joyd[0]/3, joyd[1]/3, joyd[2]/3, -joyd[3]/3-100 );
			puts( sendline );
			int r = sendto(sockfd,sendline, n,0, (struct sockaddr *)&servaddr,sizeof(servaddr));
			if( r != n )
			{
				fprintf( stderr, "Error: could not send.%d\n", r );
				exit( -4 );
			}
		}

		FILE * tuning = fopen( "tuning.txt", "rb" );
		int i = 0;
		if( tuning )
		{
			for( i = 0; i < sizeof( sendline ) && !feof( tuning ) && !ferror( tuning ); i++ )
			{
				sendline[i] = fgetc( tuning );
			}
			fclose( tuning );
		}
		if( i )
		{
			int r = sendto(sockfd,sendline, i,0, (struct sockaddr *)&servaddr,sizeof(servaddr));
			if( r != i )
			{
					fprintf( stderr, "Error: could not send tuning.%d\n", r );
					exit( -4 );
			}
		}


		usleep(10000);

	}
/*	while (fgets(sendline, 10000,stdin) != NULL)
	{
		sendto(sockfd,sendline,strlen(sendline),0,
			(struct sockaddr *)&servaddr,sizeof(servaddr));
		n=recvfrom(sockfd,recvline,10000,0,NULL,NULL);
		recvline[n]=0;
		fputs(recvline,stdout);
	}*/


}

