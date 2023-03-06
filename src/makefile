# This builds 'minibones' in one go.

# You need 'find', 'sed' and 'xargs' installed.  It relies on 'ccache' or
# something similar for fast recompilation of individual object files.

COMPILE=g++ -Wall -ggdb3 -DEXPERT

# The maximum number of parallel processes used for compilation.
# Lower it if (with minimum 1) in case compilation needs to much memory.

CORES=32

all:
	notangle -L -RCoreBased.hh CoreBased.nw >CoreBased.hh
	notangle -L -RCoreBased.cc CoreBased.nw >CoreBased.cc
	cd bbred; notangle -L -RBBRed.hh BBRed.nw >BBRed.hh
	cd bbred; notangle -L -RBBRed.cc BBRed.nw >BBRed.cc
	find . -name \*.cc | \
	  sed -e '/SimpSolver/d;/Main.cc/d;/bbdred\/main.cc/d' | \
	  xargs -n 1 -P $(CORES) $(COMPILE) -c -I. -Ibbred -Iminisat
	$(COMPILE) -o minibones *.o -lz

clean:
	rm -f CoreBased.hh CoreBased.cc bbred/BBRed.hh bbred/BBRed.cc
	rm -f makefile *.o minibones
