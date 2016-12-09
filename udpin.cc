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
  
  MUSIC::ContOutputPort* wavedata =
    setup->publishContOutput ("out");

  MPI::Intracomm comm = setup->communicator ();
  int nProcesses = comm.Get_size (); // how many processes are there?
  if (nProcesses > 1)
    {
      std::cout << "udpin: needs to run in a single MPI process\n";
      exit (1);
    }

  // Declare what data we have to export
  MUSIC::ArrayData dmap (&buffer.keysPressed,
			 MPI_DOUBLE,
			 0,
			 OurUDPProtocol::KEYBOARDSIZE);
  wavedata->map (&dmap, 1); // buffer only one tick
  
  double stoptime;
  setup->config ("stoptime", &stoptime);

  // Setup socket
  struct sockaddr_in si_other;
  int s, i;
  unsigned int slen=sizeof(si_other);
  
  if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("udpin: couldn't create socket");

  memset((char *) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons (OurUDPProtocol::TOMUSICPORT);
  if (inet_aton (OurUDPProtocol::MIDISERVER_IP, &si_other.sin_addr) == 0)
    throw std::runtime_error ("udpout: inet_aton() failed");

  startBuffer.magicNumber = OurUDPProtocol::MAGIC;
  startBuffer.stopTime = stoptime;
  if (sendto (s, &startBuffer, sizeof(startBuffer), 0, (struct sockaddr *) &si_other, slen) == -1)
    throw std::runtime_error ("udpin: failed to send start message");

  // Not really necessary since static memory is zero from start
  for (int i = 0; i < OurUDPProtocol::KEYBOARDSIZE; ++i)
    buffer.keysPressed[i] = 0.0;

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, OurUDPProtocol::TIMESTEP);

  // Simulation loop
  for (; runtime->time () < stoptime; runtime->tick ()) {
    double timeStamp;

    do {
      if (recvfrom (s, &buffer, sizeof(buffer), 0, (struct sockaddr *) &si_other, &slen)
	  == -1)
	std::runtime_error ("udpin: recvfrom()");
      timeStamp = buffer.timestamp;
    } while (timeStamp + DELAY < runtime->time ());
  }

  close (s);

  runtime->finalize ();

  delete runtime;

  return 0;
}
