/*
 *  udpin.cc
 *
 *  Copyright (C) 2016 Mikael Djurfeldt <mikael@djurfeldt.com>
 *
 *  music-osc is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libneurosim is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <mpi.h>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <music.hh>

#include "OurUDPProtocol.hh"

// This is the modeled time delay. We maintain
// t == UDP timestamp + DELAY.

#define DELAY OurUDPProtocol::TIMESTEP

struct OurUDPProtocol::toMusicPackage buffer;
struct OurUDPProtocol::startPackage startBuffer;

int
main (int argc, char* argv[])
{
  MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);
  
  MPI::Intracomm comm = setup->communicator ();

  if (comm.Get_size () > 1) {
    std::cerr << "udpin: needs to run in a single MPI process\n";
    exit (1);
  }

  MUSIC::ContOutputPort* keyPort =
    setup->publishContOutput ("out");

  // Declare what data we have to export
  MUSIC::ArrayData dataMap (&buffer.keysPressed,
			    MPI_DOUBLE,
			    0,
			    OurUDPProtocol::KEYBOARDSIZE);
  keyPort->map (&dataMap, 1); // buffer only one tick
  
  MUSIC::ContOutputPort* commandPort =
    setup->publishContOutput ("commands");

  // Declare data to export
  MUSIC::ArrayData commandMap (&buffer.commandKeys,
			       MPI_DOUBLE,
			       0,
			       OurUDPProtocol::COMMANDKEYS);
  commandPort->map (&commandMap, 1); // buffer only one tick
  
  double stoptime;
  if (!setup->config ("stoptime", &stoptime)) {
    std::cerr << "stoptime must be set in the MUSIC config file" << std::endl;
    exit(1);
  }

  // Setup socket
  int udpSocket;
  struct sockaddr_in udpAddress;
  unsigned int udpAddressSize=sizeof(udpAddress);
  
  if ((udpSocket = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("udpin: couldn't create socket");

  memset((char *) &udpAddress, 0, sizeof(udpAddress));
  udpAddress.sin_family = AF_INET;
  udpAddress.sin_port = htons (OurUDPProtocol::TOMUSICPORT);
  if (inet_aton (OurUDPProtocol::MIDISERVER_IP, &udpAddress.sin_addr) == 0)
    throw std::runtime_error ("udpout: inet_aton() failed");

  startBuffer.magicNumber = OurUDPProtocol::MAGICTOMUSIC;
  startBuffer.stopTime = stoptime;
  if (sendto (udpSocket, &startBuffer, sizeof(startBuffer), 0,
	      (struct sockaddr *) &udpAddress, udpAddressSize)
      == -1)
    throw std::runtime_error ("udpin: failed to send start message");

  // Not really necessary since static memory is zero from start
  std::fill(buffer.keysPressed.begin(), buffer.keysPressed.end(), 0.0);

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, OurUDPProtocol::TIMESTEP);

  // Simulation loop
  for (; runtime->time () < stoptime; runtime->tick ()) {
    double timeStamp;

    do {
      if (recvfrom (udpSocket, &buffer, sizeof(buffer), 0,
		    (struct sockaddr *) &udpAddress, &udpAddressSize)
	  == -1)
	std::runtime_error ("udpin: recvfrom()");
      timeStamp = buffer.timestamp;
    } while (timeStamp + DELAY < runtime->time ());
  }

  close (udpSocket);

  runtime->finalize ();

  delete runtime;

  return 0;
}
