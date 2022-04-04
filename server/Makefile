##==================================================
## Specific variables for  LINUX
##--------------------------------------------------
## uncomment the following lines if you are
## using GNU Linux

LDFLAGS=
CFLAGS=-Wall -O2 -DLINUX=1

##==================================================
##==================================================
## Specific variables for  SOLARIS
##--------------------------------------------------
## uncomment the following lines if you are
## using SUN Solaris

#LDFLAGS=-lsocket
#CFLAGS=-Wall -O2 -DSOLARIS=1

##==================================================

ntalks: ntalks.cpp
	g++ -g -o ntalks ntalks.cpp $(LDFLAGS) $(CFLAGS)

install:
	cp ntalks /usr/local/bin

clean:
	rm -f ntalks
