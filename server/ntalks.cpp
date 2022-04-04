/*
 *  ntalks.cpp
 *
 *  This file is part of ntalk chat server distribution
 *  Copyright (C) 2001 by Krzysztof Czuba (kab@interia.pl)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <iostream>
#include <cstring>  	// C++ string
#include <list>
using namespace std;

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#ifdef SOLARIS
#include <sys/filio.h>
#endif
#include <signal.h>

#define VERSION "1.0.0"

#define MESSAGE 	"01"
#define JOIN 		"02"
#define LEAVE 		"03"
#define DISCONNECT 	"04"
#define SHUTDOWN 	"05"
#define ONLINE 		"06"
#define AWAY 		"07"
#define AWAYOFF 	"08"
#define SERVER 		"09"
#define WHOIS 		"10"
#define SRVMESSAGE 	"11"


struct user {
    string nick;
    string addr;
    string since;
    int fd;
};

list<user> user_list;

int server_sockfd, client_sockfd;
int server_len, client_len;
int fd_count;
int result, x, online_count, already_sigint;
char msg_r[512], msg_s[512];

fd_set readfds, testfds;

sockaddr_in server_address, client_address;


void add_fd(int fd)
{
    user u;
    u.nick = "";
    u.fd = fd;

    user_list.push_back(u);
    fd_count++;
}

void del_fd(int fd)
{
    list<user>::iterator i;
    for(i = user_list.begin(); i != user_list.end(); ++i)
    {
	user u = *i;

	if(u.fd == fd)
	{
	    user_list.erase(i);
	    fd_count--;
	    return;
	}
    }
}

void add_nick(int fd, string nick, string addr, string since)
{
    list<user>::iterator i;
    for(i = user_list.begin(); i != user_list.end(); ++i)
    {
	if(i->fd == fd)
	{
	    i->nick = nick;
	    i->addr = addr;
	    i->since = since;
	    return;
	}
    }
}

bool check_nick(string nick)
{
    list<user>::const_iterator i;
    for(i = user_list.begin(); i != user_list.end(); ++i)
    {
	if(i->nick == nick) return 0;
    }

    return 1;
}

void send_all(string messg)
{
    strcpy(msg_s, messg.c_str());

    list<user>::const_iterator i;
    for(i = user_list.begin(); i != user_list.end(); ++i)
    {
	write(i->fd, &msg_s, sizeof(msg_s));
    }
}

void send_fd(const string messg, int fd)
{
    strcpy(msg_s, messg.c_str());
    write(fd, &msg_s, sizeof(msg_s));
}

user find_user(string nick)
{
    list<user>::const_iterator i;
    for(i = user_list.begin(); i != user_list.end(); ++i)
    {
	user u = *i;
	if(u.nick == nick) return u;
    }

    user us;
    us.fd = -1;
    return us;
}

string time()
{
    tm *tm;
    time_t now;
    char tt[9];
    string strtt;

    time(&now);
    tm = localtime(&now);
    strftime(tt, 9, "%H:%M:%S", tm);
    strtt = tt;

    return strtt;
}

string str_online()
{
    string str = "server@server:06:fd:";

    list<user>::iterator i;
    for(i = user_list.begin(); i != user_list.end(); ++i)
    {
	str.append(i->nick);
	str.append(";");
    }

    str.append("\n");

    return str;
}

string create_message_string(string com,
			     string data,
			     string sender = "server@server",
			     string to = "all")
{
    string m = sender;
    m.append(":" + com + ":");
    m.append(to + ":");
    m.append(data);
    m.append("\n");

    return m;
}

void parse_command(const string messg, int fd)
{
    string msg = messg;
    string nick, addr, com, to, data;

    // sender@host:01:all:message

    nick = msg.substr(0, msg.find('@'));
    msg.erase(0, msg.find('@') + 1);
    addr = msg.substr(0, msg.find(':'));
    msg.erase(0, msg.find(':') + 1);
    com = msg.substr(0, msg.find(':'));
    msg.erase(0, msg.find(':') + 1);
    to  = msg.substr(0, msg.find(':'));
    msg.erase(0, msg.find(':') + 1);
    data = msg.substr(0, msg.find('\n'));

    if(com.compare(MESSAGE) == 0)
    {
	send_all(messg);
    }
    else if(com.compare(JOIN) == 0)
    {
	if(!check_nick(nick))
	{
	    string str = create_message_string(DISCONNECT, "Nick already in use");
	    send_fd(str, fd);
	    return;
	}

	add_nick(fd, nick, addr, time());
	send_all(messg);
	send_all(str_online());
    }
    else if(com.compare(LEAVE) == 0)
    {
	send_all(messg);
    }
    else if(com.compare(SERVER) == 0)
    {
	string str = create_message_string(SERVER, "ntalk server " VERSION);
	send_fd(str, fd);
    }
    else if(com.compare(ONLINE) == 0)
    {
	string str = str_online();
	send_fd(str, fd);
    }
    else if(com.compare(AWAY) == 0)
    {
	send_all(messg);
    }
    else if(com.compare(AWAYOFF) == 0)
    {
	send_all(messg);
    }
    else if(com.compare(WHOIS) == 0)
    {
	user u = find_user(data);
	if(u.fd == -1)
	{
	    string tmpstr = data + ": No such user";
	    string str = create_message_string(WHOIS, tmpstr);
	    send_fd(str, fd);
	}
	else
	{
	    string tmpstr = u.nick + "@" + u.addr;
	    string str = create_message_string(WHOIS, u.since, tmpstr);
	    send_fd(str, fd);
	}
    }
}

static void signal_interrupt(int sig)
{
    string msg = create_message_string(SHUTDOWN, "Server shutdown");

    if (!already_sigint)
    {
	already_sigint = 1;
	send_all(msg);
	cout << "Server shut down" << endl;
	exit(0);
    }
}

int main(int argc, char *argv[])
{
    int opt;

    while((opt = getopt(argc, argv, "v")) != -1)
    {
	if(opt == 'v')
	{
	    cout << "version " VERSION "\n";
	    exit(0);
	}

	else exit(0);
    }

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(9734);
    server_len = sizeof(server_address);

    bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
    listen(server_sockfd, 5);

    online_count = 0;
    fd_count = 0;

    cout << "Running ntalk server " VERSION "\n";

    already_sigint = 0;
    signal(SIGINT, signal_interrupt);

    FD_ZERO(&readfds);
    FD_SET(server_sockfd, &readfds);
    while(1)
    {
	int fd;
	int nread;

	testfds = readfds;

	result = select(FD_SETSIZE, &testfds, (fd_set *)0,
			(fd_set *)0, (struct timeval *) 0);

	if(result < 1)
	{
	    cerr << "server: select";
	    exit(1);
	}

	for(fd = 0; fd < FD_SETSIZE; fd++)
	{
	    if(FD_ISSET(fd, &testfds))
	    {
		if(fd == server_sockfd)
		{
		    client_sockfd = accept(server_sockfd, (sockaddr *)&client_address, (socklen_t *)&client_len);
		    FD_SET(client_sockfd, &readfds);
		    online_count++;
		    add_fd(client_sockfd);
		}
		else
		{
		    ioctl(fd, FIONREAD, &nread);
		    if(nread == 0)
		    {
			close(fd);
			FD_CLR(fd, &readfds);
			online_count--;
			del_fd(fd);
			send_all(str_online());
		    }

		    else
		    {
			read(fd, &msg_r, sizeof(msg_r));
			string msgg = msg_r;
			parse_command(msgg, fd);
		    }
		}
	    }
	}
    }
}
