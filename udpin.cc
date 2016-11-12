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

// This is the expected time difference between incoming UDP packets
// as well as the MUSIC inter-tick-interval

#define TIMESTEP 0.001

// This is the modeled time delay. We maintain
// t == UDP timestamp + DELAY.

#define DELAY TIMESTEP

#define SRV_IP "127.0.0.1"
#define PORT 9931

#if 1
typedef float real;
#define MPI_MYREAL MPI::FLOAT
#else
typedef double real;
#define MPI_MYREAL MPI::DOUBLE
#endif

int
main (int argc, char* argv[])
{
  MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);
  
  int width = atoi (argv[1]); // command line arg gives width

  MUSIC::ContOutputPort* wavedata =
    setup->publishContOutput ("out");

  MPI::Intracomm comm = setup->communicator ();
  int nProcesses = comm.Get_size (); // how many processes are there?
  if (nProcesses > 1)
    {
      std::cout << "udpin: needs to run in a single MPI process\n";
      exit (1);
    }

  int nLocalVars = width;
  real* data = new real[nLocalVars];
  for (int i = 0; i < nLocalVars; ++i)
    data[i] = 0.0;
    
  // Declare what data we have to export
  MUSIC::ArrayData dmap (data,
			 MPI_MYREAL,
			 0,
			 nLocalVars);
  wavedata->map (&dmap, 1); // buffer only one tick
  
  double stoptime;
  setup->config ("stoptime", &stoptime);

  // Setup socket
  struct sockaddr_in si_other;
  int s, i;
  unsigned int slen=sizeof(si_other);
  int dataSize = nLocalVars * sizeof (real);
  int buflen = sizeof (real) + dataSize;
  real buf[1 + nLocalVars];

  if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("udpin: couldn't create socket");

  memset((char *) &si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons (PORT);
  if (inet_aton (SRV_IP, &si_other.sin_addr) == 0)
    throw std::runtime_error ("udpout: inet_aton() failed");

  buf[0] = 4712.0;
  buf[1] = stoptime;
  if (sendto (s, buf, 2 * sizeof (real), 0, (struct sockaddr *) &si_other, slen) == -1)
    throw std::runtime_error ("udpin: failed to send start message");
  
  for (int i = 0; i <= nLocalVars; ++i)
    buf[i] = 0.0;

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, TIMESTEP);

  // Simulation loop
  for (; runtime->time () < stoptime; runtime->tick ())
    {
      real timeStamp;
      do
	{
	  if (recvfrom (s, buf, buflen, 0, (struct sockaddr *) &si_other, &slen)
	      == -1)
	    std::runtime_error ("udpin: recvfrom()");
	  timeStamp = buf[0];
	}
      while (timeStamp + DELAY < runtime->time ());
      memcpy (data, &buf[1], dataSize);
    }

  close (s);

  runtime->finalize ();

  delete runtime;

  return 0;
}
