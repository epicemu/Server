
HCC=$(shell ccache -V 2>/dev/null)
ifneq (,$(findstring version,$(HCC)))
	CC=ccache gcc
else
	CC=gcc
endif

OUT=-o 
LINKOUT=-o 
NOLINK=-c
LINKER=gcc
MYSQL_FLAGS=$(shell mysql_config --cflags)
MYSQL_LIB=$(shell mysql_config --libs)
DFLAGS=-DEQDEBUG=5
WFLAGS=-Wall -Wuninitialized -Wwrite-strings -Wcast-qual -Wno-deprecated  -Wcomment -Wcast-align
COPTS=$(WFLAGS) -O -g -march=i686 -pthread -pipe -DEQLAUNCH $(DFLAGS) $(MYSQL_FLAGS)
LINKOPTS=$(COPTS) -rdynamic -L. -lstdc++ -ldl $(MYSQL_LIB)

all: eqlaunch

include .depend

include makefile.common

.depend depend:
	echo $(SF) "" | sed -e 's#.obj/#../common/#g' -e 's/\.o/.cpp/g' | \
	xargs gcc -MM $(COPTS) >.depend

