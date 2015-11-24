/******************************************************************************
  Author: Geoffrey Pitman
  Created: 11/18/2015
  Due: 12/2/2015
  Course: CSC328 
  Professor: Dr. Frye
  Assignment: #9
  Filename: dl_client.cpp
  Purpose: Create a concurrent TCP file transfer server
  Compile: gcc dl_client.cpp               :  on ACAD (csit)
           or use make file
  Command Line: argv[0] == executable file :  required
                argv[1] == host            :  required
				argv[2] == port            :  optional - defaults to 50118
                Ex:  ./dl_client 127.0.0.1
                     ./dl_client csit.acad.kutztown 7000
					 
  ---NOTE--- If server terminates unexpectedly while client is waiting
             for user input, the client will not recognize the disconnection
			 until the user enters valid input.  Invalid input will keep
			 the user in the validation loop.  Once it sees valid input
			 it will stop hanging the disconnect is immediately detected. 
 *****************************************************************************/

#include <netinet/in.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cerrno>
#include <fcntl.h>
#include <string>
#include <cstring>
#include <clocale>
#include <cstdlib>
#include <arpa/inet.h>
#include <netdb.h>

#define SIZE sizeof(struct sockaddr_in) 

using namespace std;


// lookup_IP does a reverse lookup on hostname and sets ip to point at the 
//     ip address string - however - if an ip is passed, it will come out
//     unchanged
// ip is passed by reference. nothing is returned
void lookup_IP(char* hostname, char* ip);

int main(int argc, char *argv[])
{
	int port;
	// check command line args
	if(argc < 2 || argc > 3)
    	{ 
       		printf("\nUsage: %s <host> <[port]>\n", argv[0]);
       		return -1;
    	} // end if
	else if (argv[2])
	{
		stringstream argin(argv[2]);
		argin >> port;
	}
	else
		port = 50118;

  	int b = 0;
  	int dlstart = 0;
  	int sockfd;
  	char c[256];
  	char buf[2000];
  	char ip[128];
  	bool dlmode = false;
  	bool firstpass = false;
  	string status = "";
  	string getin = "";
  	string file = "";
  	string check = "";
  	ifstream inf;
  	ofstream outf;

  	// initialize sockaddr struct
  	struct sockaddr_in server = {AF_INET, htons(port)};
  
  	// reverse lookup hostname and set ip address 
  	lookup_IP(argv[1], ip);  
  
	// convert and store the server's IP address
  	server.sin_addr.s_addr = inet_addr(ip);
  
  	// set up the transport end point
  	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    	{
      		perror("socket call failed");
      		exit(-1);
    	} 

  	// connect the socket to the server's address
  	if (connect(sockfd, (struct sockaddr *)&server, SIZE) == -1)
    	{
      		perror("connect call failed");
      		exit(-1);
    	}

	if((b = read(sockfd, buf, 2000)) < 0)
	{
		perror("read call failed: ");
		exit(-1);
	}
	buf[b] = 0;
	if(b)
		cout << "[SERVER MESSAGE] " << buf << endl;
 
  // send and receive information with the server
  	while(1)
  	{
		bzero(buf, 2000);
		b = 0;
		
		// dlmode starts false
		// switch to dlmode when client confirms it is ready for download
		while (dlmode)
		{
			// let server know that it is receiving transfer
			getin = "READY";	
			b = write(sockfd, getin.c_str(), strlen(getin.c_str()));
			if(b < 0)
			{
				perror("Write call failed: ");
				exit(-1);
			}	
			else if(b == 0)
			{
				cout << "Server disconnected unexpectedly..." << endl;
				break;
			}
			
			// receive file line by line
			b = read(sockfd, buf, 2000);
			if(b < 0)
			{
				perror("Read call failed: ");
				exit(-1);
			}
			else if(b == 0)
			{
				cout << "Server disconnected unexpectedly..." << endl;
				break;	
			}
			// process file flags
			else
			{
				buf[b] = 0;
				check = buf;
				// blank line string is processed
				if(check == "!@#$%^&*()_+")
					outf << endl;
				// indicates normal progression
				else if(check == "READY")
					continue;
				// indicates transfer is complete
				// clean up and switch out of dlmode
				else if (check == "DONE")
				{
					dlmode = false;
					outf.close();
					cout << "Transfer complete!" << endl;
				}
			    //write to file
				else
					outf << buf << endl;	
			}
		}
		
		// user input validation loop
		while (1)
		{
			// command prompt for user
			cout << ">>";
			getline(cin, getin);
			stringstream str(getin);
			string temp;
			str >> temp;
		
			// validate commands
			if(temp.length() < 1)
				continue;
			else if (temp != "CD" && temp != "DOWNLOAD" && temp != "DIR" && temp != "BYE")
			{
				cout << "Invalid command!\n";
				continue;
			}
			if (temp == "DOWNLOAD")
				str >> file;
			
			break;
		}
		if (getin == "BYE")
			break;
		
		// send data to server
		b = write(sockfd, getin.c_str(), strlen(getin.c_str()));
		if(b < 0)
		{
			perror("Write call failed: ");
			exit(-1);
		}
		else if(b == 0)
		{
			cout << "Server disconnected unexpectedly..." << endl;
			break;
		}
		
		// receive data from server
		b = read(sockfd, buf, 2000);
		if(b < 0)
		{
			perror("Read call failed: ");
			exit(-1);
		}
		else if(b == 0)
		{
			
			cout << "Server disconnected unexpectedly..." << endl;
			break;	
		}
		else
		{
			buf[b] = 0;
			string check(buf);
			// process server response to client's download command
			if (check == "READY")
			{
				// check if there is already a file with same name as 
				//     potential download
				int rval = access(file.c_str(), F_OK);
				// if file exists, ask if user wants to overwrite.
				if(rval == 0)
				{
					cout << "Do you really want to overwrite <" << file
					     << ">?(y/n)";
					char in;
					cin >> in;
					// if they want to over write, set dlmode flag true
					// open file and prepare for download
					if (in == 'Y' || in == 'y')
					{
						dlmode = true;
						cout << "Downloading file..." << endl;
						outf.open(file.c_str());
					}
					// otherwise abort transfer
					else
					{
						cout << "Transfer canceled!" << endl;
						getin = "STOP";	
					}
				}
				// record errors if file exists but is write protected
				else if(errno == ENOENT)
				{
					dlmode = true;
					cout << "Downloading file..." << endl;
					outf.open(file.c_str());
				}
			}
			// output server message if we haven't swithced to dlmode
			string red(buf);
			if (getin != "STOP" && red != "READY")
				cout << "[SERVER MESSAGE] " << buf << endl;
		}
   }

	close(sockfd);
	cout << "Client shutting down...Goodbye!" << endl;
	return 0;
}

// lookup_IP does a reverse lookup on hostname and sets ip to point at the 
//   ip address string
void lookup_IP(char* hostname, char* ip)
{
    struct hostent* he;
    struct in_addr** addr_list;
	
    // do lookup
    if ((he = gethostbyname(hostname)) == NULL)
    {
        herror("error: hostname could not be resolved\n");
        exit(-1);
    } // end if

    // set ip
    addr_list = (struct in_addr**) he->h_addr_list;
    strcpy(ip, inet_ntoa(*addr_list[0]));
}
