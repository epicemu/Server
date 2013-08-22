
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
DFLAGS=-DEQDEBUG=5 -DCATCH_CRASH -DNO_PIDLOG -DSHAREMEM -DFIELD_ITEMS -DCOMBINED -DAPP_OPCODE_SIZE=2 -Di386
#try commenting out the following three lines to disable embedded perl
PERL_FLAGS=$(shell perl -MExtUtils::Embed -e ccopts)
PERL_LIB=$(shell perl -MExtUtils::Embed -e ldopts)
DFLAGS+=-DEMBPERL -DEMBPERL_PLUGIN -DHAS_UNION_SEMUN
WFLAGS=-Wall -Wuninitialized -Wwrite-strings -Wcast-qual -Wno-deprecated  -Wcomment -Wcast-align
COPTS=$(WFLAGS) -O -g -march=i686 -pthread -pipe -D_GNU_SOURCE -DINVERSEXY -DFX -DZONE $(DFLAGS) $(MYSQL_FLAGS) $(PERL_FLAGS)
LINKOPTS=$(COPTS) -rdynamic -L. -lstdc++ -ldl $(MYSQL_LIB) $(PERL_LIB)

all: zone

include .depend

include makefile.common

.depend depend:
	for f in $(SF); \
	do \
		i=`echo $$f | sed -e 's#.obj/#../common/#g' -e 's/\.o/.cpp/g' `; \
		gcc -MM $(COPTS) $$i | sed "s#^[^ ]*:#$$f:#g"; \
	done  >.depend

wine:
	$(MAKE) -f makefile.wine

