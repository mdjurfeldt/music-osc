/*
 *  udpout.cc
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
#include <music.hh>
#include <cstring>
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "OurUDPProtocol.hh"

#define DELAY 0.0

struct OurUDPProtocol::fromMusicPackage buffer;
struct OurUDPProtocol::startPackage startBuffer;


int
main (int args, char* argv[])
{
  MUSIC::Setup* setup = new MUSIC::Setup (args, argv);

  MPI::Intracomm comm = setup->communicator ();

  if (comm.Get_size () > 1) {
    std::cerr << "udpout: needs to run in a single MPI process\n";
    exit (1);
  }

  // Initialize UDP socket
  int udpSocket;
  struct sockaddr_in udpAddress;
  unsigned int udpAddressSize = sizeof (udpAddress);

  if ((udpSocket = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("udpout: couldn't create socket");

  memset((char *) &udpAddress, 0, sizeof(udpAddress));
  udpAddress.sin_family = AF_INET;
  udpAddress.sin_port = htons (OurUDPProtocol::FROMMUSICPORT);
  if (inet_aton(OurUDPProtocol::MIDISERVER_IP, &udpAddress.sin_addr) == 0)
    throw std::runtime_error ("udpout: inet_aton() failed");

  MUSIC::ContInputPort* keyPort =
    setup->publishContInput ("in");

  // Declare where in memory to put data
  MUSIC::ArrayData dmap (&buffer.keysPressed,
			 MPI_DOUBLE,
			 0,
			 OurUDPProtocol::KEYBOARDSIZE);
  keyPort->map (&dmap, DELAY, 1, false);

  double stoptime;
  if (!setup->config ("stoptime", &stoptime)) {
    std::cerr << "stoptime must be set in the MUSIC config file" << std::endl;
    exit(1);
  }

  startBuffer.magicNumber = OurUDPProtocol::MAGICFROMMUSIC;
  startBuffer.stopTime = stoptime - OurUDPProtocol::TIMESTEP;
  if (sendto (udpSocket, &startBuffer, sizeof (startBuffer), 0,
	      (struct sockaddr *) &udpAddress, udpAddressSize)
      == -1)
    throw std::runtime_error ("udpout: failed to send start message");

  // Not really necessary since static memory is zero from start
  std::fill(buffer.keysPressed.begin(), buffer.keysPressed.end(), 0.0);

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, OurUDPProtocol::TIMESTEP);
  for (; runtime->time () < stoptime; runtime->tick ()) {
    buffer.timestamp = runtime->time ();
    if (sendto (udpSocket, &buffer, sizeof(buffer), 0,
		(struct sockaddr *) &udpAddress, udpAddressSize)
	== -1)
      throw std::runtime_error ("udpout: sendto()");
  }

  runtime->finalize ();
  
  delete runtime;

  return 0;
}
