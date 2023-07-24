// -----------------------------------------------------
// file MultiClient.h declares client and server classes
// using example from Beej's Guide to Network Programming
// https://beej.us/guide/bgnet/html/#a-simple-stream-server
//
// M. Williamsen, 24 July 2023
// -----------------------------------------------------

// client base class, subclass to add special behavior
class client
{
public:
	client(int sock, char *port, struct sockaddr_storage addr);
	void serialize(ostream &);
	
private:
	void startClient();
	thread clientThread;
	int sock;		// socket for connecting
	char *port;
	struct sockaddr_storage addr;	
};

// server base class, subclass to add special behavior
class server
{
public:
    server(char *arg);
    void serialize(ostream &);
    
private:
    int startServer(void);
    char *port;
    thread serverThread;
    list<client *> theList;
    int theSock;	// socket for listening
};
