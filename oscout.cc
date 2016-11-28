/*
 *  oscout.cc
 *
 *  Copyright (C) 2016 Mikael Djurfeldt <mikael@djurfeldt.com>
 *                     and Gerhard Eckel <eckel@kth.se>
 *
 *  based on udpout.c by Mikael Djurfeldt
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

#include "lo/lo.h"

#define TIMESTEP 0.001
#define DELAY 0.0

#define SCPORT "57121"

#if 0
typedef float real;
#define MPI_MYREAL MPI::FLOAT
#else
typedef double real;
#define MPI_MYREAL MPI::DOUBLE
#endif

int main (int args, char* argv[])
{
  MUSIC::Setup* setup = new MUSIC::Setup (args, argv);

  char *port = argv[1];

  MUSIC::ContInputPort* keydata = setup->publishContInput ("in");
  MPI::Intracomm comm = setup->communicator ();
  int nProcesses = comm.Get_size ();
  
  if (nProcesses > 1) {
    std::cout << "udpout: needs to run in a single MPI process\n";
    exit (1);
  }
  
  int width = 0;
  if (keydata->hasWidth ())
    width = keydata->width ();
  else
    comm.Abort (1);

  lo_address address = lo_address_new(NULL, port);
  lo_blob blob = lo_blob_new(width, NULL);
  char *blobdata = (char*)lo_blob_dataptr(blob);
  lo_timetag timetag = {0, 0}; // so far unused
  int msgCount = 0;
  
  real buf[width];
  MUSIC::ArrayData dmap (buf, MPI_MYREAL, 0, width);
  keydata->map (&dmap, DELAY, 1, false);
  
  double stoptime;
  setup->config ("stoptime", &stoptime);
  
  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, TIMESTEP);

  for (; runtime->time () < stoptime; runtime->tick ()) {   
    lo_message message = lo_message_new();
    lo_bundle bundle = lo_bundle_new(timetag);
    
    for (int i = 0; i < width; i++) {
      blobdata[i] = buf[i];
    }
    
    lo_message_add_int32(message, msgCount++);
    lo_message_add_double(message, runtime->time());
    lo_message_add_blob(message, blob);
    lo_bundle_add_message(bundle, "/keystate", message);
    
    if (lo_send_bundle(address, bundle) == -1)
      throw std::runtime_error ("oscout: send_bundle");
    
    lo_message_free(message);
    lo_bundle_free(bundle);
  }
  
  lo_address_free(address);
  lo_blob_free(blob);
  
  runtime->finalize ();
  
  delete runtime;
  
  return 0;
}
