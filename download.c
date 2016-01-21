#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>             // stl vector
#include <fcntl.h>
#include <sstream>
#include <iostream>

#define SOCKET_ERROR        -1
#define BUFFER_SIZE         100
#define HOST_NAME_SIZE      255
#define MAX_MSG_SZ      	1024

using namespace std;

// Determine if the character is whitespace
bool isWhitespace(char c)
{ switch (c)
    {
        case '\r':
        case '\n':
        case ' ':
        case '\0':
            return true;
        default:
            return false;
    }
}

// Strip off whitespace characters from the end of the line
void chomp(char *line)
{
    int len = strlen(line);
    while (isWhitespace(line[len]))
    {
        line[len--] = '\0';
    }
}

// Read the line one character at a time, looking for the CR
// You dont want to read too far, or you will mess up the content
char * GetLine(int fds)
{
    char tline[MAX_MSG_SZ];
    char *line;
    
    int messagesize = 0;
    int amtread = 0;
    while((amtread = read(fds, tline + messagesize, 1)) < MAX_MSG_SZ)
    {
        if (amtread >= 0)
            messagesize += amtread;
        else
        {
            perror("Socket Error is:");
            fprintf(stderr, "Read Failed on file descriptor %d messagesize = %d\n", fds, messagesize);
            exit(2);
        }
        //fprintf(stderr,"%d[%c]", messagesize,message[messagesize-1]);
        if (tline[messagesize - 1] == '\n')
            break;
    }
    tline[messagesize] = '\0';
    chomp(tline);
    line = (char *)malloc((strlen(tline) + 1) * sizeof(char));
    strcpy(line, tline);
    //fprintf(stderr, "GetLine: [%s]\n", line);
    return line;
}
    
// Change to upper case and replace with underlines for CGI scripts
void UpcaseAndReplaceDashWithUnderline(char *str)
{
    int i;
    char *s;
    
    s = str;
    for (i = 0; s[i] != ':'; i++)
    {
        if (s[i] >= 'a' && s[i] <= 'z')
            s[i] = 'A' + (s[i] - 'a');
        
        if (s[i] == '-')
            s[i] = '_';
    }
    
}


// When calling CGI scripts, you will have to convert header strings
// before inserting them into the environment.  This routine does most
// of the conversion
char *FormatHeader(char *str, const char *prefix)
{
    char *result = (char *)malloc(strlen(str) + strlen(prefix));
    char* value = strchr(str,':') + 1;
    UpcaseAndReplaceDashWithUnderline(str);
    *(strchr(str,':')) = '\0';
    sprintf(result, "%s%s=%s", prefix, str, value);
    return result;
}

// Get the header lines from a socket
//   envformat = true when getting a request from a web client
//   envformat = false when getting lines from a CGI program

void GetHeaderLines(vector<char *> &headerLines, int skt, bool envformat)
{
    // Read the headers, look for specific ones that may change our responseCode
    char *line;
    char *tline;
    
    tline = GetLine(skt);
    while(strlen(tline) != 0)
    {
        if (strstr(tline, "Content-Length") || 
            strstr(tline, "Content-Type"))
        {
            if (envformat)
                line = FormatHeader(tline, "");
            else
                line = strdup(tline);
        }
        else
        {
            if (envformat)
                line = FormatHeader(tline, "HTTP_");
            else
            {
                line = (char *)malloc((strlen(tline) + 10) * sizeof(char));
                sprintf(line, "%s", tline);                
            }
        }
        //fprintf(stderr, "Header --> [%s]\n", line);
        
        headerLines.push_back(line);
        free(tline);
        tline = GetLine(skt);
    }
    free(tline);
}

void  download(int argc, char* argv[], bool debug, bool count, int &successes)
{
    int hSocket;                 /* handle to socket */
    struct hostent* pHostInfo;   /* holds info about a machine */
    struct sockaddr_in Address;  /* Internet socket address stuct */
    long nHostAddress;
    char pBuffer[BUFFER_SIZE];
    unsigned nReadAmount;
    char strHostName[HOST_NAME_SIZE];
    int nHostPort;

    if(argc < 3)
      {
        printf("\nUsage: download [-rc] host-name host-port url\n");
        exit(0);
      }
    else
      {
      	if(argc > 5) {
			strcpy(strHostName,argv[3]);
			nHostPort=atoi(argv[4]);
      	}
      	else if(argc > 4) {
			strcpy(strHostName,argv[2]);
			nHostPort=atoi(argv[3]);
        } else {
        	strcpy(strHostName,argv[1]);
			nHostPort=atoi(argv[2]);
		}
      }

    /* make a socket */
    hSocket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

    if(hSocket == SOCKET_ERROR)
    {
        printf("\nCould not make a socket\n");
        exit(0);
    }

    /* get IP address from name */
    pHostInfo=gethostbyname(strHostName);
    /* copy address into long */
    memcpy(&nHostAddress,pHostInfo->h_addr,pHostInfo->h_length);

    /* fill address struct */
    Address.sin_addr.s_addr=nHostAddress;
    Address.sin_port=htons(nHostPort);
    Address.sin_family=AF_INET;

    /* connect to host */
    int connectValue = connect(hSocket,(struct sockaddr*)&Address,sizeof(Address));
    if(connectValue == SOCKET_ERROR)
    {
        printf("\nCould not connect to host\n");
        exit(0);
    }
    
    char *message = (char *)malloc(MAX_MSG_SZ);
    if(argc > 5) {
    	sprintf(message, "GET %s HTTP/1.1\r\nHOST:%s:%s\r\n\r\n",argv[5],argv[3],argv[4]);
    } else if(argc > 4) {
    	sprintf(message, "GET %s HTTP/1.1\r\nHOST:%s:%s\r\n\r\n",argv[4],argv[2],argv[3]);
    } else {
		sprintf(message, "GET %s HTTP/1.1\r\nHOST:%s:%s\r\n\r\n",argv[3],argv[1],argv[2]);
    }
    if(!count) {
    	printf("\n");
    }
    write(hSocket,message,strlen(message));
    memset(pBuffer, 0, BUFFER_SIZE);
	
	vector<char *> headerLines;
	char buffer[MAX_MSG_SZ];
	char contentType[MAX_MSG_SZ];
	
	// First read the status line
	char *startline = GetLine(hSocket);
	string code = startline;
	if(code == "HTTP/1.1 200 OK") {
		successes++;
	}

	// Read the header lines
	GetHeaderLines(headerLines, hSocket , false);

	// Now print them out
	if(debug && !count) {
		printf("%s\n",startline);
		for (int i = 0; i < headerLines.size(); i++) {
			printf("%s\n",headerLines[i]);
			if(strstr(headerLines[i], "Content-Type")) {
					 sscanf(headerLines[i], "Content-Type: %s", contentType);
			}
		}
		printf("\n\n");
	}

	// Now read and print the rest of the file
	if(!count) {
		int rval;
		while((rval = read(hSocket,buffer,MAX_MSG_SZ)) > 0) {
			write(1,buffer,rval);
		}
	}
   
    /* close socket */                       
    if(close(hSocket) == SOCKET_ERROR)
    {
        printf("\nCould not close socket\n");
        exit(0);
    }
    free(message);
}

int  main(int argc, char* argv[])
{
	int c;
	bool debug = false;
	bool count = false;
	int countValue = 1;
	int successes = 0;
	
	while ((c = getopt (argc, argv, "dc:")) != -1) {
		switch (c) {
			case 'd':
			debug = true;
			break;
			case 'c':
			count = true;
			countValue = atoi(optarg);
			break;
			case '?':
			if (optopt == 'c')
          		fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        	else if (isprint (optopt))
				fprintf (stderr, "Unknown option `-%c'.\n", optopt);
			else
				fprintf (stderr,
					   "Unknown option character `\\x%x'.\n",
					   optopt);
				return 1;
			default:
				abort ();
		}
	}
	for(int i = 0; i < countValue; i++) {
		download(argc, argv, debug, count, successes);
	}
	if(count) {
		printf("Succeeded %d times\n", successes);
	}
	return 0;
}