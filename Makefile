CXX ?= g++
MPICXX ?= mpicxx

CXXFLAGS ?= -g -O3 -std=c++11 -DHAVE_CLOCK_NANOSLEEP

LOFLAGS = -I$$HOME/local/include -L$$HOME/local/lib
PBCPNNBASE=../pbcpnn

.PHONY: all mpi server test

all: mpi server test

server: miditoudp udptomidi

mpi: udpin udpout

test: simproxy filetobrain braintofile

udpin: udpin.cc OurUDPProtocol.hh
	$(MPICXX) $(CXXFLAGS) -o udpin udpin.cc -lmusic

udpout: udpout.cc OurUDPProtocol.hh
	$(MPICXX) $(CXXFLAGS) -o udpout udpout.cc -lmusic

udptoosc: udptoosc.cc
	$(CXX) $(CXXFLAGS) -o udptoosc udptoosc.cc

osctoudp: osctoudp.cc rtclock.o rtclock.h
	$(CXX) $(CXXFLAGS) -o osctoudp rtclock.o osctoudp.cc -lpthread

rtclock.o: rtclock.h rtclock.cpp
	$(CXX) $(CXXFLAGS) -c rtclock.cpp

simproxy: simproxy.cc OurUDPProtocol.hh
	$(MPICXX) $(CXXFLAGS) -o simproxy simproxy.cc -lmusic

filetobrain: filetobrain.cc OurUDPProtocol.hh
	$(MPICXX) $(CXXFLAGS) -o filetobrain filetobrain.cc -lmusic

braintofile: braintofile.cc
	$(MPICXX) $(CXXFLAGS) -o braintofile braintofile.cc -lmusic

oscout: oscout.cc
	$(MPICXX) $(CXXFLAGS) -o oscout $(LOFLAGS) oscout.cc -lmusic -llo

oscin: oscin.cc
	$(MPICXX) $(CXXFLAGS) -o oscin $(LOFLAGS) oscin.cc -lmusic -llo

minimalbrain: minimalbrain.cpp
	$(MPICXX) $(CXXFLAGS) -fopenmp -I ${PBCPNNBASE} -I ${PBCPNNBASE}/build-music -o minimalbrain $(LOFLAGS) minimalbrain.cpp -L ${PBCPNNBASE}/build-music/base -lpbcpnn -lmusic

udptomidi: udptomidi.cpp OurUDPProtocol.hh
	${CXX}  $(CXXFLAGS) -I /usr/include/rtmidi -o udptomidi udptomidi.cpp -l rtmidi

miditoudp: miditoudp.cpp OurUDPProtocol.hh rtclock.h rtclock.o
	${CXX}  $(CXXFLAGS) -I /usr/include/rtmidi -o miditoudp  rtclock.o miditoudp.cpp -l rtmidi -l pthread
