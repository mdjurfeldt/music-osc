CXX ?= g++
MPICXX ?= mpicxx

CXXFLAGS ?= -g -O3 -DHAVE_CLOCK_NANOSLEEP

.PHONY: all mpi server test

all: mpi server test

server: udptoosc osctoudp

mpi: udpin udpout

test: simproxy filetobrain braintofile

udpin: udpin.cc
	$(MPICXX) $(CXXFLAGS) -o udpin udpin.cc -lmusic

udpout: udpout.cc
	$(MPICXX) $(CXXFLAGS) -o udpout udpout.cc -lmusic

udptoosc: udptoosc.cc
	$(CXX) $(CXXFLAGS) -o udptoosc udptoosc.cc

osctoudp: osctoudp.cc rtclock.o
	$(CXX) $(CXXFLAGS) -o osctoudp rtclock.o osctoudp.cc -lpthread

rtclock.o: rtclock.h rtclock.cpp
	$(MPICXX) $(CXXFLAGS) -c rtclock.cpp

simproxy: simproxy.cc
	$(MPICXX) $(CXXFLAGS) -o simproxy simproxy.cc -lmusic

filetobrain: filetobrain.cc
	$(MPICXX) $(CXXFLAGS) -o filetobrain filetobrain.cc -lmusic

braintofile: braintofile.cc
	$(MPICXX) $(CXXFLAGS) -o braintofile braintofile.cc -lmusic

