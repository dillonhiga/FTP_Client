#This is a hack to pass arguments to the run command and probably only 
#works with gnu make. 
ifeq (run,$(firstword $(MAKECMDGOALS)))
  # use the rest as arguments for "run"
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # ...and turn them into do-nothing targets
  $(eval $(RUN_ARGS):;@:)
endif


all: Ftp

#The following lines contain the generic build options
CC=gcc
CPPFLAGS=
CFLAGS=-g -Werror-implicit-function-declaration

#List all the .o files here that need to be linked 
OBJS=Ftp.o usage.o dir.o 

usage.o: usage.c usage.h

dir.o: dir.c dir.h

Ftp.o: Ftp.c dir.h usage.h Ftp.h

Ftp: $(OBJS) 
	$(CC) -o Ftp $(OBJS) 

clean:
	rm -f *.o
	rm -f Ftp

### ignore the below, for the hack above
.PHONY: run
run: Ftp  
	./Ftp $(RUN_ARGS)
