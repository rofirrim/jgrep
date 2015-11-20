PROGRAMS=jit-add jit-add-or-sub jit-sum jgrep-basic jgrep-jit ptr-arith jgrep-concurrent

# Put here where you have your GCC installation that supports libgccjit
GCCDIR=

JITCFLAGS=-I$(GCCDIR)/include
JITLDFLAGS=-L$(GCCDIR)/lib -Wl,-rpath,$(GCCDIR)/lib
JITLIBS=-lgccjit -lpthread

## EXTRAE_CFLAGS=-I$(HOME)/soft/extrae/install/include -DEXTRAE_SUPPORT
## EXTRAE_LIBS=-L$(HOME)/soft/extrae/install/lib -lpttrace

CC=gcc
CFLAGS=-O2 -Wall -g $(JITCFLAGS) $(EXTRAE_CFLAGS) -std=gnu11
LDFLAGS=$(JITLDFLAGS) $(JITLIBS) $(EXTRAE_LIBS)

all: $(PROGRAMS)

.PHONY: clean
clean:
	rm -f *.o
	rm -f $(PROGRAMS)
