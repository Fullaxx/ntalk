/*
 *  ntalk.cpp
 *
 *  This file is part of ntalk chat client distribution
 *  Copyright (C) 2001 by Krzysztof Czuba (kab@interia.pl)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#define _REENTRANT	/* required by pthread */

#include <iostream>
#include <fstream>
#include <cstring> 	// C++ string
#include <algorithm>
using namespace std;

#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <ncurses.h>

#define VERSION "1.0.0"
#define MAX_DATA_SIZE 512

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


struct color {
    int you;
    int other;
    int nick;
    int bracket;
    int status;
};

struct message {
    string leave;
    string away;
};

WINDOW *input_w, *main_w, *sts_w, *count_w, *user_w;

void *thread(void *);

int sockfd;
int len;
int result;
int tmp;
int already_sigint;
bool away_mode;
string nickname;
string tmpaddr;
string hostname;
string msgs;
string msgr;
sockaddr_in address;
pthread_t my_thread;
char msg_s[MAX_DATA_SIZE];
char msg_r[MAX_DATA_SIZE];
color cl;
message ms;


void update_all()
{
    wnoutrefresh(main_w);
    wnoutrefresh(input_w);
    wnoutrefresh(sts_w);
    wnoutrefresh(count_w);
    wclear(input_w);
    doupdate();
}

void update_main()
{
    wnoutrefresh(main_w);
    doupdate();
    wrefresh(input_w);
}

void gui_init()
{
    initscr();
    main_w = newwin(LINES - 2, COLS - 21, 1, 0);
    sts_w = newwin(1, COLS - 21, 0, 0);
    count_w = newwin(1, 20, 0, COLS - 20);
    input_w = newwin(1, COLS, LINES - 1, 0);
    user_w = newwin(LINES - 2, 20, 1, COLS - 20);

    scrollok(main_w, TRUE);
    scrollok(input_w, FALSE);
    keypad(input_w, TRUE);

    update_all();
}

void color()
{
    if(!has_colors())
    {
	endwin();
	cerr << "ntalk: ncurses: has_colors() failed" << endl;
	cerr << "This terminal doesn't support colors. Try to use different." << endl;
	exit(1);
    }

    if(start_color() != OK)
    {
	endwin();
	cerr << "ntalk: ncurses: start_color() failed" << endl;
	cerr << "Ncurses could not start its color engine. Try to use different terminal." << endl;
	exit(2);
    }

    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_WHITE, COLOR_BLACK);
    init_pair(3, COLOR_WHITE, COLOR_BLUE);
    init_pair(4, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_WHITE, COLOR_RED);
}

void read_config()
{
    ifstream cfile;
    string filename;
    passwd *pwd_entry;
    char buf[255];

    pwd_entry = getpwuid(getuid());
    filename = pwd_entry->pw_dir;
    filename.append("/.ntalkrc");

    cfile.open(filename.c_str());
    if(!cfile)
    {
	cerr << "ntalk: " << filename << ": Could not open configuration file for reading." << endl;
	cerr << "ntalk: Using defaults." << endl;
	return;
    }

    while(!cfile.eof())
    {
	cfile.getline(buf, 255);
	string str = buf;

	if(str[0] == '#') continue;
	else if(str[0] == 'N')
	{
	    str.erase(0, 2);
	    nickname = str;
	}
	else if(str[0] == 'H')
	{
	    str.erase(0, 2);
	    tmpaddr = str;
	}
	else if(str[0] == 'C')
	{
	    str.erase(0, 2);
	    if(str[0] == 'Y')
	    {
		str.erase(0, 2);
		cl.you = atoi(str.c_str());
	    }
	    else if(str[0] == 'O')
	    {
		str.erase(0, 2);
		cl.other = atoi(str.c_str());
	    }
	    else if(str[0] == 'N')
	    {
		str.erase(0, 2);
		cl.nick = atoi(str.c_str());
	    }
	    else if(str[0] == 'B')
	    {
		str.erase(0, 2);
		cl.bracket = atoi(str.c_str());
	    }
	    else if(str[0] == 'S')
	    {
		str.erase(0, 2);
		cl.status = atoi(str.c_str());
	    }
	}
	else if(str[0] == 'M')
	{
	    str.erase(0, 2);
	    if(str[0] == 'L')
	    {
		str.erase(0, 2);
		ms.leave = str;
	    }
	    else if(str[0] == 'A')
	    {
		str.erase(0, 2);
		ms.away = str;
	    }
	}
    }

    cfile.close();
}

void save_config()
{
    fstream cfile, newcfile;
    string filename, newfilename, str;
    passwd *pwd_entry;
    char buf[255];

    pwd_entry = getpwuid(getuid());
    filename = pwd_entry->pw_dir;
    filename.append("/.ntalkrc");
    newfilename = "/tmp/ntalkrc_tmp";

    cfile.open(filename.c_str(), ios::in);
    if(!cfile)
    {
	cerr << "ntalk: "<< filename << ": Could not open configuration file for reading." << endl;
	return;
    }

    newcfile.open(newfilename.c_str(), ios::out);
    if(!newcfile)
    {
	cerr << "ntalk: " << newfilename << ": Could not open file for writing." << endl;
	return;
    }

    while(!cfile.eof())
    {
	strcpy(buf, "");
	cfile.getline(buf, 255);

	switch(buf[0]) {
	    case '#':
		newcfile << buf << endl;
		break;
	    case 'N':
		newcfile << "N=" << nickname << endl;
		break;
	    case 'H':
		newcfile << "H=" << tmpaddr << endl;
		break;
	    case 'C':
		if(buf[2] == 'Y')
		    newcfile << "C:Y=" << cl.you << endl;
		else if(buf[2] == 'O')
		    newcfile << "C:O=" << cl.other << endl;
		else if(buf[2] == 'N')
		    newcfile << "C:N=" << cl.nick << endl;
		else if(buf[2] == 'B')
		    newcfile << "C:B=" << cl.bracket << endl;
		else if(buf[2] == 'S')
		    newcfile << "C:S=" << cl.status << endl;
		break;
	    case 'M':
		if(buf[2] == 'L')
		    newcfile << "M:L=" << ms.leave << endl;
		else if(buf[2] == 'A')
		    newcfile << "M:A=" << ms.away << endl;
		break;
	    default:
		newcfile << buf << endl;
	}
    }

    cfile.close();
    newcfile.close();

    cfile.open(filename.c_str(), ios::out);
    newcfile.open(newfilename.c_str(), ios::in);

    while(!newcfile.eof())
    {
	newcfile.getline(buf, 255);
	cfile << buf << endl;
    }

    newcfile.close();
    cfile.close();
    unlink(newfilename.c_str());
}

string createStr(string com, string data, string to = "all")
{
    string m;
    m = nickname;
    m.append("@");
    m.append(hostname + ":");
    m.append(com + ":");
    m.append(to + ":");
    m.append(data);
    m.append("\n");

    return m;
}

void away_off()
{
    msgs = createStr(AWAYOFF, "I'm back.");
    strcpy(msg_s, msgs.c_str());

    write(sockfd, &msg_s, sizeof(msg_s));
    away_mode = 0;
    nocbreak();
    echo();
    mvwprintw(sts_w, 0, 0, "ntalk - user: %s\n", nickname.c_str());
    wrefresh(sts_w);
}

void refresh_user_w(const string messg)
{
    string msg = messg;
    int a, x;

    wclear(user_w);
    wrefresh(user_w);

    a = count(msg.begin(), msg.end(), ';');
    for(x = 0; x < a; x++)
    {
	// msg.replace(msg.find(';'), 1, '\n');// ntalk-client-0.6.3 originally used this, when I tried to compile it gave me this error:
	// ntalk.cpp: In function `void refresh_user_w(std::string)':
	// ntalk.cpp:345: error: invalid conversion from `char' to `const char*'
	// ntalk.cpp:345: error:   initializing argument 3 of `std::basic_string<_CharT, _Traits, _Alloc>& std::basic_string<_CharT, _Traits, _Alloc>::replace(typename _Alloc::size_type, typename _Alloc::size_type, const _CharT*) [with _CharT = char, _Traits = std::char_traits<char>, _Alloc = std::allocator<char>]'
	// make: *** [ntalk] Error 1
	// 
	// Added tempcstr for ntalk-client-1.0.0 and then changed the replace line (hope its right)
	// This removes the compile error but I'm not sure if it is the right way to fix this.
	char tempcstr[2];
	tempcstr[0] = '\n';
	tempcstr[1] = '\0';
	msg.replace(msg.find(';'), 1, tempcstr);
    }

    mvwprintw(user_w, 0, 0, "%s", msg.c_str());
    wrefresh(user_w);
}

void parse_user_command(const char *com)
{
    string _com = com;
    string c = _com.substr(0, _com.find(' '));
    string arg = _com.erase(0, _com.find(' ') + 1);

    wattron(sts_w, COLOR_PAIR(cl.status));

    if(c.compare("/q") == 0)
    {
	msgs = createStr(LEAVE, ms.leave);
	strcpy(msg_s, msgs.c_str());
	write(sockfd, &msg_s, sizeof(msg_s));
	endwin();
	save_config();
	exit(0);
    }
    else if(c.compare("/help") == 0)
    {
	mvwprintw(sts_w, 0, 0, "See README for more information\n");
	wrefresh(sts_w);
	wattron(main_w, COLOR_PAIR(2));
	wprintw(main_w, "-----------------------------------------------\n");
	wprintw(main_w, "Edit your ~/.ntalkrc to change default messages\n");
	wprintw(main_w, "and color schemes.\n");
	wprintw(main_w, "-----------------------------------------------\n");
	wprintw(main_w, "/help         - command list\n");
	wprintw(main_w, "/server       - information about server\n");
	wprintw(main_w, "/clear        - clear main window\n");
	wprintw(main_w, "/sclear       - clear status bar\n");
	wprintw(main_w, "/away         - switch to 'away' mode\n");
	wprintw(main_w, "/online       - refresh users box\n");
	wprintw(main_w, "/whois [nick] - simple information about user\n");
	wprintw(main_w, "/about        - about ntalk\n");
	wprintw(main_w, "/q            - quit ntalk\n");
	wprintw(main_w, "-----------------------------------------------\n");
	wnoutrefresh(main_w);
	doupdate();
    }
    else if(c.compare("/online") == 0)
    {
	msgs = createStr(ONLINE, "");
	strcpy(msg_s, msgs.c_str());
	write(sockfd, &msg_s, sizeof(msg_s));
    }
    else if(c.compare("/away") == 0)
    {
	msgs = createStr(AWAY, ms.away);
	strcpy(msg_s, msgs.c_str());
	write(sockfd, &msg_s, sizeof(msg_s));

	mvwprintw(sts_w, 0, 0, "Press 'x' to exit away mode.\n");
	wrefresh(sts_w);
	away_mode = 1;
	cbreak();
	noecho();
	while(away_mode == 1)
	{
	    tmp = wgetch(input_w);
	    if(tmp == 'x') away_off();
	}
    }
    else if(c.compare("/clear") == 0)
    {
	wclear(main_w);
	update_main();
    }
    else if(c.compare("/sclear") == 0)
    {
	mvwprintw(sts_w, 0, 0, "ntalk %s\n", VERSION);
	wrefresh(sts_w);
    }
    else if(c.compare("/about") == 0)
    {
	mvwprintw(sts_w, 0, 0, "Ncurses Talk, version %s by Krzysztof Czuba <kab@interia.pl>\n", VERSION);
	wrefresh(sts_w);
    }
    else if(c.compare("/server") == 0)
    {
	msgs = createStr(SERVER, "");
	strcpy(msg_s, msgs.c_str());
	write(sockfd, &msg_s, sizeof(msg_s));
    }
    else if(c.compare("/whois") == 0)
    {
	if(c.compare(arg) == 0)
	{
	    mvwprintw(sts_w, 0, 0, "Command usage: /whois [nick]\n");
	    wrefresh(sts_w);
	    return;
	}

	msgs = createStr(WHOIS, arg);
	strcpy(msg_s, msgs.c_str());
	write(sockfd, &msg_s, sizeof(msg_s));
    }
    else
    {
	wattron(sts_w, COLOR_PAIR(cl.status));
	mvwprintw(sts_w, 0, 0, "No such command: %s\n", c.c_str());
	wrefresh(sts_w);
    }
}

void parse_server_command(const string messg)
{
    string msg = messg;
    const string c_msg = messg;
    string nick, addr, com, to, data;

    nick = msg.substr(0, msg.find('@'));
    msg.erase(0, msg.find('@') + 1);
    addr = msg.substr(0, msg.find(':'));
    msg.erase(0, msg.find(':') + 1);
    com = msg.substr(0, msg.find(':'));
    msg.erase(0, msg.find(':') + 1);
    to  = msg.substr(0, msg.find(':'));
    msg.erase(0, msg.find(':') + 1);
    data = msg.substr(0, msg.find('\n'));

    wattron(main_w, COLOR_PAIR(2));

    if(com.compare(MESSAGE) == 0)
    {
	wattron(main_w, COLOR_PAIR(cl.bracket));
	wprintw(main_w, "<");
	wattron(main_w, COLOR_PAIR(cl.nick));
	wprintw(main_w, "%s", nick.c_str());
	wattron(main_w, COLOR_PAIR(cl.bracket));
	wprintw(main_w, ">");
	if(nick.compare(nickname) == 0) wattron(main_w, COLOR_PAIR(cl.you));
	else wattron(main_w, COLOR_PAIR(cl.other));
	wprintw(main_w, " %s\n", data.c_str());
	update_main();
    }
    else if(com.compare(JOIN) == 0)
    {
	//wprintw(main_w, "---> %s@%s has joined the room\n", nick.c_str(), addr.c_str());
	wprintw(main_w, "---> %s joined the room\n", nick.c_str());
	update_main();

	msgs = createStr(ONLINE, "");
	strcpy(msg_s, msgs.c_str());
	write(sockfd, &msg_s, sizeof(msg_s));
    }
    else if(com.compare(LEAVE) == 0)
    {
	wprintw(main_w, "---> %s left the room (%s)\n", nick.c_str(), data.c_str());
	update_main();
    }
    else if(com.compare(SERVER) == 0)
    {
	wprintw(main_w, "---> %s\n", data.c_str());
	update_main();
    }
    else if(com.compare(AWAY) == 0)
    {
	wprintw(main_w, "---> %s is away from computer: %s\n", nick.c_str(), data.c_str());
	update_main();
    }
    else if(com.compare(AWAYOFF) == 0)
    {
	wprintw(main_w, "---> %s is back.\n", nick.c_str());
	update_main();
    }
    else if(com.compare(ONLINE) == 0)
    {
	refresh_user_w(data);
    }
    else if(com.compare(DISCONNECT) == 0)
    {
	endwin();
	cout << "\nDisconnected: " << data << endl;
	exit(0);
    }
    else if(com.compare(WHOIS) == 0)
    {
	wprintw(main_w, "---> %s is: %s@%s (since %s)\n", nick.c_str(), nick.c_str(), addr.c_str(), data.c_str());
	update_main();
    }
    else if(com.compare(SRVMESSAGE) == 0)
    {
	wprintw(main_w, "---> %s\n", data.c_str());
	update_main();
    }
    else if(com.compare(SHUTDOWN) == 0)
    {
	save_config();
	endwin();
	cout << "ntalk: Server shutdown" << endl;
	exit(0);
    }
}

string get_host(string hostname)
{
    char **addr;
    hostent *host_info;

    host_info = gethostbyname(hostname.c_str());
    if(!host_info)
    {
	cerr << "ntalk: gethostbyname() failed" << endl;
	exit(1);
    }

    addr = host_info->h_addr_list;
    string str = inet_ntoa(*(in_addr *)*addr);

    return str;
}

static void signal_interrupt(int sig)
{
    msgs = createStr(LEAVE, "Interrupted...");
    strcpy(msg_s, msgs.c_str());

    if(!already_sigint)
    {
	already_sigint = 1;
	write(sockfd, &msg_s, sizeof(msg_s));
	endwin();
	exit(0);
    }
}

int main(int argc, char *argv[])
{
    int opt;
    passwd *pwd_entry;
    char hostn[256];

    // Some defaults...

    tmpaddr = "127.0.0.1";
    pwd_entry = getpwuid(getuid());
    nickname = pwd_entry->pw_name;

    cl.you 	= 1;
    cl.other 	= 2;
    cl.nick 	= 2;
    cl.bracket 	= 4;
    cl.status   = 3;

    ms.leave = "Leaving...";
    ms.away = "Busy...";

    read_config();

    while((opt = getopt(argc, argv, "n:h:v?")) != -1)
    {
	if(opt == 'h' && optarg != "") tmpaddr = optarg;
	else if(opt == 'n' && optarg != "")
	{
	    if((strlen(optarg) > 21) || (strlen(optarg) < 2))
	    {
		cout << "Error: Your nick must contain 2 - 20 charackters\n";
		exit(0);
	    }
	    nickname = optarg;
	}
	else if(opt == 'v')
	{
	    cout << "version " << VERSION << " (ncurses 5.2)\n(C) Krzysztof Czuba <kab@interia.pl>\n";
	    exit(0);
	}
	else if(opt == '?')
	{
	    cout << "\nUsage: client [option]\n\nOptions:\n"
                   "-h\t\tspecify server address\n"
		   "-n\t\tspecify nickname\n"
                   "-v\t\tshow version information\n"
		   "-?\t\tdisplay this information\n\n";
	    exit(0);
	}
	else exit(0);
    }

    if(gethostname(hostn, 255) == -1)
    {
	cerr << "ntalk: gethostname() failed" << endl;
	cerr << "Ntalk could not obtain your computer's name." << endl;
	exit(0);
    }

    hostname = hostn;

    tmpaddr = get_host(tmpaddr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(tmpaddr.c_str());
    address.sin_port = htons(9734);
    len = sizeof(address);
    result = connect(sockfd, (sockaddr *)&address, len);

    if(result == -1)
    {
	endwin();
	cerr << "ntalk: connect() failed" << endl;
	cerr << "ntalk: Make sure that the address is valid and the server is running" << endl;
	exit(1);
    }

    gui_init();
    color();

    wattron(sts_w, COLOR_PAIR(cl.status));
    wattron(count_w, COLOR_PAIR(3));
    mvwprintw(sts_w, 0, 0, "ntalk %s - user: %s | type /help for command list\n", VERSION, nickname.c_str());
    mvwprintw(count_w, 0, 0, "      Online:      \n");
    wrefresh(sts_w);
    wrefresh(count_w);

    msgs = createStr(JOIN, "");
    strcpy(msg_s, msgs.c_str());
    write(sockfd, &msg_s, sizeof(msg_s));

    already_sigint = 0;
    signal(SIGINT, signal_interrupt);

    if(pthread_create(&my_thread, NULL, thread, (void *)"watek") != 0)
    {
	cerr << "ntalk: pthread_create() failed" << endl;
	cerr << "Could not create thread." << endl;
	exit(1);
    }

    while(1)
    {
	char input[MAX_DATA_SIZE];
	wgetnstr(input_w, input, MAX_DATA_SIZE);
	input[MAX_DATA_SIZE - 1] = '\0';
	if(input[0] == '/')
	{
	    parse_user_command(input);
	    wprintw(input_w, "\n");
	    continue;
	}

	msgs = createStr(MESSAGE, string(input));
	strcpy(msg_s, msgs.c_str());

	write(sockfd, &msg_s, sizeof(msg_s));
	wprintw(input_w, "\n");
    }

    close(sockfd);
    exit(0);
}

void *thread(void *watek)
{
    while(1)
    {
	read(sockfd, &msg_r, sizeof(msg_r));
	string msg = msg_r;
	parse_server_command(msg);
    }
}
