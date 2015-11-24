/******************************************************************************
  Author: Geoffrey Pitman
  Created: 11/18/2015
  Due: 12/2/2015
  Course: CSC328 
  Professor: Dr. Frye
  Assignment: #9
  Filename: dl_server.cpp
  Purpose: Create a concurrent TCP file transfer server
  Compile: gcc dl_server.cpp -lpthread     :  on ACAD (csit)
           or use make file
  Command Line: argv[0] == executable file :  required
                argv[1] == port            :  optional - defaults to 50118
                Ex:  ./dl_server
                     ./dl_server 7000 
 *****************************************************************************/

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <clocale>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#define MAXBUF 2049
#define	QLEN 32
#define SIZE sizeof(struct sockaddr_in)

using namespace std;

// handles ctrl-c kill signal
// cleans up and exits program
void sig_handler(int signo);

// automatically executes when thread is created
// detailed documentation in the function
// this is where the bulk work for the client is done
int	slave_work(int ssock);	

// list just the files in a directory
// returns a formatted string of the list
string listdir(DIR* dir);

// master thread's sock descriptor
static int msock;

int main(int argc, char *argv[])
{
	pthread_t th;
	int port;
	int qlen = QLEN;
	int ssock;	
	struct sockaddr_in server = {AF_INET, htons(port), INADDR_ANY};
	static struct sigaction act;
	
	// register sig handler and mask pipe error sig
	act.sa_handler = sig_handler;
	sigfillset(&(act.sa_mask));
	sigaction(SIGPIPE, &act, NULL);
	
	// check command line args
	if(argc > 2)
    { 
       printf("\nUsage: %s <[port]>\n", argv[0]);
       return -1;
    } // end if
	else if (argv[1])
	{
		stringstream argin(argv[1]);
		argin >> port;
	}
	else
		port = 50118;
	
	// listen for signal
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("\nCouldn't catch kill sig(ctrl-c)\n");
	
	// set up the transport end point
	if ((msock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      perror("Socket call failed");
      exit(-1);
    }

	// bind and address to the end point
	if (bind(msock, (struct sockaddr *)&server, SIZE) < 0)
    {
      perror("Bind call failed");
      exit(-1);
    }

	// start listening for incoming connections
	if (listen(msock, qlen) < 0)
    {
      perror("Listen call failed");
      exit(-1);
    }   
	
	
	cout << "Server started...listening for incoming connections" << endl;
	// handle concurrency
	// accept incoming connection and create new thread to handle it
	while (1) 
	{
		if ((ssock = accept(msock, NULL, NULL)) < 0) 
		{
			perror("Accept call failed");
			continue;
		}

		if (pthread_create(&th, NULL, (void * (*)(void *))slave_work,
		    (void *)(long)ssock) < 0) 
			perror("Thread create call failed");
	}
	
	
	// the program should never fall through to the end of main...
	//     but just in case
	close(msock);
	
	return 0;
}

// handles ctrl-c signal by closing server/cleaning up
void sig_handler(int signo)
{
	if (signo == SIGINT)
	{
		// clean up
		close(msock);
		printf("\nServer terminated\n");
		// kill program
		exit(signo);
	}
}

// list just the files in a directory
// returns a formatted string of the list
string listdir(DIR* dir)
{
	struct dirent *entry;
	string f = "\n###start listing###";
	
    if (!(entry = readdir(dir) ) )
        return "@error@";

	// iterate through entries
    do{
        if (entry->d_type != DT_DIR)
		{
            f += "\n";
		    f += entry->d_name;
		}
    } while (entry = readdir(dir));
    
	return (f += "\n###end listing###");
}

// automatically executes when thread is created
// this is where the bulk work of the client gets done
int slave_work(int ssock)
{
	// identifies connected client
	cout << "Client thread created with ID: " << pthread_self() << endl;
	
	int start = 0;
	int dlstart = 0;
	bool dlmode = false;
	string path = "/";
	string pt, temp, filename;
	ifstream inf;
	DIR *dir = opendir("/");
		
	// handle initial contact with client	
	if (start == 0)
	{
		char hello[] = "Server says 'HELLO'";
		write(ssock, hello, strlen(hello));
		start = 1;
	}
	
	// main processing loop
	while (1)
	{
		char buf[MAXBUF];
		int b = 0;
		bzero(buf, MAXBUF);
		string temp = "";
		
		// dlmode starts false
		// switch to dlmode when client confirms it is ready for download
		while(dlmode)
		{	
			// make sure client is receiving download
			b = read(ssock, buf, MAXBUF);
			if(b < 0)
			{
				perror("Read call failed: ");
				exit(-1);
			}
			else if(b == 0)
			{		
				cout << "client disconnected mid-transfer..." << endl;
				break;	
			}
			else
			{
				buf[b] = 0;
				string check(buf);
				if (check != "READY")
				{
					dlmode = false;
					inf.close();
					break;
				}
			}
			
			// read in file to be transferred line by line
			// when it hits eof it closes file and switches out of dlmode
			if(!getline(inf, temp))
			{
				temp = "DONE";
				inf.close();	
				dlstart = 0;
				dlmode = false; 
			}
			// special format string to recognize blank lines
			else if (temp.length() < 1)
				temp = "!@#$%^&*()_+";
			
			// send file line by line
			b = write(ssock, temp.c_str(), strlen(temp.c_str()));
			if(b < 0)
			{
				perror("Write call failed: ");
				exit(-1);
			}
			else if(b == 0)
			{
				cout << "client disconnected mid-transfer..." << endl;
				break;
			}
		}
		
		// main handler for incomming data
		b = read(ssock, buf, MAXBUF);
		if(b < 0)
		{
			perror("Read call failed: ");
			break;
		}
		else if(b == 0)
		{
			cout << "Client disconnected...";
			break;
		}
		else
		{
			buf[b] = 0;
			stringstream str(buf);
			str >> temp;
			
			// handle BYE case
			// break out of main loop and fall through to clean up
			if(temp == "BYE")
			{	
				cout << "Client disconnected...";
				break;
			}
			// handle DIR case
			// print only files from the current directory
			else if(temp == "DIR")
			{ 
				temp = "";
				temp = listdir(dir);
				dir = opendir(path.c_str());
				
				// no files if length is less than 1
				if (temp.length() < 1)
					temp = "Directory contains no files";
			}
			// handle CD case
			else if(temp == "CD")
			{
				pt = path;
				str >> temp;
				
				//build path string
				if (temp[0] == '/')
					pt = temp;
				else if(temp.length() == 1 && temp[0] == '.')
					pt = temp;
				else if(pt[pt.length()-1] != '/')
					pt += "/" + temp;
				else
					pt += temp;
			
			     // attempt to open directory
				 // record errors or success
				if ( (dir = opendir(pt.c_str())) <= 0)
				{
					temp = strerror(errno);
					dir = opendir(path.c_str());
				}
				else
				{
					path = pt;
					temp = "Current directory is now: " + path;
				}
			}
			// handle DOWNLOAD case
			// initiates steps to enter dlmode
			else if(temp == "DOWNLOAD")
			{
				pt = path;
				str >> temp;
				
				if(pt != ".")
					pt += "/" + temp;
				else
					pt = temp;
				
				// attempt to open file
				// on success, tell client that it is ready to start transfer,
				//      or record error
				inf.open(pt.c_str());
				if(inf.is_open())
					temp = "READY";
				else
				{
					temp = "File Not Available: ";
					temp += strerror(errno);
				}
			}
			// handle the client's download confirmation
			// switch to dlmode if client confirms ready,
			//      or cancel transfer if client says stop
			else if(temp == "READY")
				dlmode = true;
			else if(temp =="STOP")
			{
				dlmode = false;
				temp = "Transfer canceled";
			}
			
		}// end read handler

		
		// sends back the processed data requested from 
		//          client message handler above
		b = write(ssock, temp.c_str(), strlen(temp.c_str()));
		temp = "";
		if(b < 0)
		{
			perror("Write call failed: ");
			break;
		}
		else if(b == 0)
		{		
			cout << "Client disconnected...";
			break;
		}
		
	}// end while	
	
	// clean up
	closedir(dir);
	close(ssock); 
	
	// notify termination of client and thread and exit
	cout << "thread terminating...ID: " << pthread_self() << endl;
	pthread_detach(pthread_self());
	pthread_exit(NULL);
	
	return ssock;
}
