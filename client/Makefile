##==================================================
## Specific variables for  LINUX
##--------------------------------------------------
## uncomment the following lines if you are
## using GNU Linux

LDFLAGS= -lncurses -lpthread
CFLAGS=-Wall -O2 -DLINUX=1

##==================================================
##==================================================
## Specific variables for  SOLARIS
##--------------------------------------------------
## uncomment the following lines if you are
## using SUN Solaris

#LDFLAGS= -lncurses -lpthread -lsocket -lnsl
#CFLAGS=-Wall -O2 -DSOLARIS=1

##==================================================

ntalk: ntalk.cpp
	g++ -g -o ntalk ntalk.cpp $(LDFLAGS) $(CFLAGS)

install:
	cp ntalk /usr/local/bin/

clean:
	rm -f ntalk
