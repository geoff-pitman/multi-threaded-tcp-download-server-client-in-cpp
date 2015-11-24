# Geoffrey Pitman
# Make file for download server/client project

all: dl_server dl_client

dl_server: dl_server.cpp
	g++ dl_server.cpp -lpthread -o dl_server

dl_client: dl_client.cpp
	g++ dl_client.cpp -o dl_client.cpp
