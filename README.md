# music-osc
Experimental interface between MUSIC and OSC

Set correct SRV_IP in all source files. Default is localhost.

To build mpi binaries:

make mpi

To build server binaries:

make server

To make test example:

make test

Running test example:

On server, launch both udptoosc and osctoudp:

./usptoosc & ./osctoudp

Launch test.music:

mpirun -np 3 music test.music

Output should look like:

udptoosc: 1.003 KEY 3 PRESSED
udptoosc: 1.003 KEY 4 PRESSED
udptoosc: 1.004 KEY 3 PRESSED
udptoosc: 1.004 KEY 4 PRESSED
udptoosc: 1.005 KEY 3 PRESSED
udptoosc: 1.005 KEY 4 PRESSED
udptoosc: 1.006 KEY 3 PRESSED
.
.
.
