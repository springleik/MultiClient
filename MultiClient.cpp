// ------------------------------------------------------
// file MultiClient.cpp defines client and server classes
// using example from Beej's Guide to Network Programming
// https://beej.us/guide/bgnet/html/#a-simple-stream-server
//
// M. Williamsen, 24 July 2023
// ------------------------------------------------------

#include <unistd.h>
#include <iostream>
#include <thread>
#include <map>
#include <list>
#include <string.h>
#include <sstream>
#include <netdb.h>
#include <arpa/inet.h>

using namespace std;

#include "MultiClient.h"

// ------------------------------------------------------
// client base class, subclass to add special behavior
client::client(int aSock, char *aPort, struct sockaddr_storage anAddr):
	sock(aSock), port(aPort), addr(anAddr)
{
	// run client in a new thread
	clientThread = thread(&client::startClient, this);
}

// send message to client
ssize_t doSend(int sockfd, const void *buf, size_t len, int flags)
{
	// result will be either -1 on error,
	// or number of bytes sent on success
	int rslt = 0, total = 0;
	do
	{
		// iterate if necessary to send whole buffer
		rslt = send(sockfd, buf, len, flags);
		if (0 > rslt) {break;}
		total += rslt;
	}
	while (total < len);
	return rslt;
}

// connection handler runs in a thread
// TODO handle multi-packet messages and replies
// found empirically that terminal programs send '\n' on enter,
// and expect '\r' when receiving lines of text.
// TODO should skip over escape characters used with telnet
void client::startClient()
{
    bool done = false;
    
    // build prompt string
    string prompt("\n");
    prompt += port;
    prompt += ": ";
    
    // handle client messages
    while (!done)
    {
        // send prompt to client
        int rslt = doSend(sock, prompt.c_str(), prompt.length(), 0);
        if (-1 == rslt) {perror("client::startClient:prompt");}
        
        // receive message from client
        // NOTE recv is blocking, won't exit on request
        stringstream ss;
        char c = '\0';
        do
        {
			// iterate until newline received
			rslt = recv(sock, &c, 1, 0);
			if (0 > rslt)
			{
				perror("client::startClient:recv");
				break;
			}
			
			// exit if connection dropped
			if (!rslt) {break;}
			
			// ignore carriage returns
			if ('\r' == c) {continue;}
			
			// ignore nulls too
			if (!c) {continue;}
			
			// accept characters other than line feed
			if (c != '\n') {ss << c;}
		}
		while (c != '\n');
        
        // exit if connection dropped
        if (!rslt) {break;}
        
        // TODO check for Json text on input
        
        // do something interesting (copying input for now)
        rslt = doSend(sock, ss.str().c_str(), ss.str().length(), 0);
        if (-1 == rslt) {perror("client::startClient:reply");}
        c = '\r';
        rslt = doSend(sock, &c, 1, 0);
        if (-1 == rslt) {perror("client::startClient:newline");}
        
        // look for commands
        if (!ss.str().compare(0, 4, "done")) {done = true;}
    }

	// client has gone offline
    // TODO remove client from list and release memory
    close(sock);    
    sock = -1;
}

void client::serialize(ostream &o)
{
	// get client information
	char str[INET6_ADDRSTRLEN];
	memset(str, 0, sizeof str);
	struct sockaddr *sa = (struct sockaddr *)&addr;
	void *v = NULL;
	short what = -1;
	switch (sa->sa_family)
	{
	case AF_INET:
		v = &(((struct sockaddr_in*)sa)->sin_addr);
		what = ((struct sockaddr_in *)sa)->sin_port;
		break;
	case AF_INET6:
		v = &(((struct sockaddr_in6*)sa)->sin6_addr);
		what = ((struct sockaddr_in6 *)sa)->sin6_port;
		break;
	default:
		perror("client::serialize:clientFamily");
	}
	if (!inet_ntop(sa->sa_family, v, str, sizeof str))
		{perror("client::serialize:clientPort");}
	
	// build output stream
	stringstream ss;
	ss << "{\"addr\":\"" << str << '"'
		<< ",\"port\":" << ntohs(what)
		<< ",\"sock\":" << sock;
		
	// get host information if connected
	if (-1 < sock)
	{
		memset(str, 0, sizeof str);
		struct sockaddr_storage xaddr;
		socklen_t asize = sizeof xaddr;
		sa = (struct sockaddr *)&xaddr;
		int rslt = getsockname(sock, sa, &asize);
		if (!rslt)
		{
			what = -1;
			switch (sa->sa_family)
			{
			case AF_INET:
				v = &(((struct sockaddr_in *)sa)->sin_addr);
				what = ((struct sockaddr_in *)sa)->sin_port;
				break;
			case AF_INET6:
				v = &(((struct sockaddr_in6 *)sa)->sin6_addr);
				what = ((struct sockaddr_in6 *)sa)->sin6_port;
				break;
			default:
				perror("client::serialize:hostFamily");
			}
			if (!inet_ntop(sa->sa_family, v, str, sizeof str))
				{perror("client::serialize:hostPort");}
			else
				{ss << ",\"host\":\"" << str << '"';}
		}
	}
	
	ss << '}';
	o << ss.str();
}

// ------------------------------------------------------
// server base class, subclass to add special behavior
server::server(char *arg): port(arg)
{
    // run server in a new thread
    serverThread = thread(&server::startServer, this);
}

// server startup runs in a thread
int server::startServer()
{   

    // set up hints
    struct addrinfo hints = {0}, *servinfo, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // use my IP

    // obtain linked list of available IP addresses
    int rv;
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        cerr << "getaddrinfo: " << gai_strerror(rv) << '\n';
        cerr << flush;
        return 1;
    }

    // loop through all the results and bind to the first we can
    const int yes = 1;
    for(p = servinfo; p; p = p->ai_next)
    {
        if ((theSock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server::startServer:socket");
            continue;
        }
        if (setsockopt(theSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("server::startServer:setsockopt");
            exit(1);
        }
        if (bind(theSock, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(theSock);
            perror("server::startServer:bind");
            continue;
        }
        break;
    }
    
    // all done with this structure
    freeaddrinfo(servinfo);
    if (!p)
    {
        cerr << "server::startServer: failed to bind\n";
        cerr << flush;
        exit(1);
    }
    
    // listen with backlog 10
    if (listen(theSock, 10) == -1)
    {
        perror("server::startServer:listen");
        exit(1);
    }

    // main accept loop
    string lis("listening on port ");
    lis += port;
    lis += '\n';
    cout << lis << flush;
    
    while (true)
    {
        // NOTE accept is blocking , won't exit on request
		char str[INET6_ADDRSTRLEN] = {0};
		struct sockaddr_storage addr;
		struct sockaddr *sa = (struct sockaddr *)&addr;
        socklen_t asize = sizeof addr;
        int newSock = accept(theSock, sa, &asize);
        if (newSock == -1)
        {
            perror("server::startServer:accept");
            continue;
        }
        
        // find client address
		void *v = NULL;
		switch (sa->sa_family)
		{
		case AF_INET:  v = &(((struct sockaddr_in*)sa)->sin_addr); break;
		case AF_INET6: v = &(((struct sockaddr_in6*)sa)->sin6_addr); break;
		default: perror("server::startServer:family");
		}
		
		// create new socket client
		// TODO we don't delete this when done, but should
        if (!inet_ntop(sa->sa_family, v, str, sizeof str))
			{perror("server::startServer:inet_ntop");}
        string rpt("connected from ");
        rpt += str;
        rpt += '\n';
        cout << rpt << flush;
        theList.push_back(new client(newSock, port, addr));
    }
}

void server::serialize(ostream &o)
{
	stringstream ss;
	ss << "{\"port\":" << port
		<< ",\"sock\":" << theSock;
	
	// iterate over client list
	if (theList.size())
	{
		ss << ",\"clients\":[";
		bool first = true;
		for (const auto &elem: theList)
		{
			if (first) {ss << "\n  "; first = false;}
			else {ss << ",\n  ";}
			elem->serialize(ss);
		}
		ss << ']';
	}
	ss << '}';
	o << ss.str();
}

// ------------------------------------------------------
// global variables
static map<char *, server *> serverMap;
static string cmd;
static bool run = true;

// show list of clients and servers on console
static void showList(ostream &o)
{
	stringstream ss;
	ss << "{\"servers\":[";
    
    // iterate over server list
    bool first = true;
    for (const auto &elem: serverMap)
    {
		if (first) {first = false;}
		else {ss << ",\n";}
		elem.second->serialize(ss);
    }
    ss << "]}\n" << flush;
    o << ss.str();
}

// show help text
static void showHelp(ostream &o)
{
    stringstream ss;
    ss << "Commands available in MultiClient server:"
        << "\n done"
        << "\n list"
        << "\n vers"
        << "\n help or ?"
        << endl;   
    o << ss.str();         
}

// show version text
static void showVersion(ostream &o)
{
	stringstream ss;
	ss << "\nMultiClient server version 00.00.06"
        << "\n built " __DATE__ ", " __TIME__ << endl;
    o << ss.str();
}   

// unknown command received
static void unKnown(ostream &o)
{
	stringstream ss;
	ss << "what: '" << cmd << "'?" << endl;
	o << ss.str();
}

// main entry point
int main(int argc, char **argv)
{
    // iterate over command line arguments
    // instantiate a server for each port number found
    while (char *arg = *++argv)
    {
		// create new socket server
		// TODO we don't delete this when done, but should
        server *s = new server(arg);
        if (s) {serverMap.insert(pair<char *, server *>(arg, s));}
    }

    // run console command prompt in main thread
    while (run)
    {
        cin >> cmd;
        if (!cmd.compare(0, 4, "done")) {run = false;}
        else if (!cmd.compare(0, 4, "list")) {showList(cout);}
        else if (!cmd.compare(0, 4, "vers")) {showVersion(cout);}
        else if (!cmd.compare(0, 4, "help")) {showHelp(cout);}
        else if (!cmd.compare(0, 1, "?"))    {showHelp(cout);}
        else {unKnown(cout);}
    }

    // NOTE this dumps all clients and servers without warning
    cout << "Closing multiple servers now.\n" << flush;
    return 0;
}
