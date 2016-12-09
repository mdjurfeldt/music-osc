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

  MUSIC::ContInputPort* wavedata =
    setup->publishContInput ("in");

  MPI::Intracomm comm = setup->communicator ();
  int nProcesses = comm.Get_size (); // how many processes are there?
  if (nProcesses > 1)
    {
      std::cout << "udpout: needs to run in a single MPI process\n";
      exit (1);
    }

  // Initialize UDP socket
  struct sockaddr_in si_other;
  int s;
  unsigned int slen = sizeof (si_other);

  if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("udpout: couldn't create socket");

  memset((char *) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons (OurUDPProtocol::FROMMUSICPORT);
  if (inet_aton(OurUDPProtocol::MIDISERVER_IP, &si_other.sin_addr) == 0)
    throw std::runtime_error ("udpout: inet_aton() failed");

  // Declare where in memory to put data
  MUSIC::ArrayData dmap (&buffer.keysPressed,
			 MPI_DOUBLE,
			 0,
			 OurUDPProtocol::KEYBOARDSIZE);
  wavedata->map (&dmap, DELAY, 1, false);

  double stoptime;
  setup->config ("stoptime", &stoptime);

  startBuffer.magicNumber = OurUDPProtocol::MAGIC;
  startBuffer.stopTime = stoptime - OurUDPProtocol::TIMESTEP;
  if (sendto (s, &startBuffer, sizeof (startBuffer), 0, (struct sockaddr *) &si_other, slen) == -1)
    throw std::runtime_error ("udpout: failed to send start message");

  // Unnecessary clearing of buffer
  for (int i = 0; i < OurUDPProtocol::KEYBOARDSIZE; ++i)
    buffer.keysPressed[i] = 0.0;

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, OurUDPProtocol::TIMESTEP);
  for (; runtime->time () < stoptime; runtime->tick ()) {
    buffer.timestamp = runtime->time ();
    if (sendto (s, &buffer, sizeof(buffer), 0, (struct sockaddr *) &si_other, slen) == -1)
      throw std::runtime_error ("udpout: sendto()");
  }

  runtime->finalize ();
  
  delete runtime;

  return 0;
}
