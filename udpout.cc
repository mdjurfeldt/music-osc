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

#define TIMESTEP 0.010
#define DELAY 0.0
#define SRV_IP "127.0.0.1"
#define PORT 9930

#if 0
typedef float real;
#define MPI_MYREAL MPI::FLOAT
#else
typedef double real;
#define MPI_MYREAL MPI::DOUBLE
#endif

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

  int width = 0;
  if (wavedata->hasWidth ())
    width = wavedata->width ();
  else
    comm.Abort (1);
  int nLocalVars = width;

  // Initialize UDP socket
  struct sockaddr_in si_other;
  int s;
  unsigned int slen = sizeof (si_other);
  int dataSize = nLocalVars * sizeof (real);
  int buflen = sizeof (real) + dataSize;
  real buf[1 + nLocalVars];

  if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("udpout: couldn't create socket");

  memset((char *) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons (PORT);
  if (inet_aton(SRV_IP, &si_other.sin_addr) == 0)
    throw std::runtime_error ("udpout: inet_aton() failed");

  real* data = &buf[1];
    
  // Declare where in memory to put data
  MUSIC::ArrayData dmap (data,
			 MPI_MYREAL,
			 0,
			 nLocalVars);
  wavedata->map (&dmap, DELAY, 1, false);

  double stoptime;
  setup->config ("stoptime", &stoptime);

  buf[0] = 4711.0;
  buf[1] = stoptime - TIMESTEP;
  if (sendto (s, buf, 2 * sizeof (real), 0, (struct sockaddr *) &si_other, slen) == -1)
    throw std::runtime_error ("udpout: failed to send start message");
  
  for (int i = 0; i <= nLocalVars; ++i)
    buf[0] = 0.0;

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, TIMESTEP);
  for (; runtime->time () < stoptime; runtime->tick ())
    {
      buf[0] = runtime->time ();
      if (sendto (s, buf, buflen, 0, (struct sockaddr *) &si_other, slen) == -1)
	throw std::runtime_error ("udpout: sendto()");
      std::cerr << ">";
    }
  runtime->finalize ();
  
  delete runtime;

  return 0;
}
