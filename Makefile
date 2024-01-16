
SERVER=mthread_server
CLIENT=mthread_client
TESTER=as2_testbench
CFLAGS=-O2 -g -Wall -pedantic -pthread

all: ${TESTER} ${CLIENT} ${SERVER}

${TESTER}: ${TESTER}.c linebuffer.c

${CLIENT}: ${CLIENT}.c

${SERVER}: ${SERVER}.c

.PHONY: test
test: all
	./${TESTER} ${CLIENT}

.PHONY: clean
clean:
	rm -rf *.o *~ ${TESTER} ${CLIENT} ${SERVER} 
