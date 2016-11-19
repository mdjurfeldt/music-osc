/*
 *  oscin.cc
 *
 *  Copyright (C) 2016 Mikael Djurfeldt <mikael@djurfeldt.com>
 *                     and Gerhard Eckel <eckel@kth.se>
 *
 *  based on udpin.c by Mikael Djurfeldt
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
#include <music.hh>

#include "lo/lo.h"

// This is the expected time difference between incoming OSC messages
// as well as the MUSIC inter-tick-interval

#define TIMESTEP 0.001

// This is the modeled time delay. We maintain
// t == OSC timestamp + DELAY.

#define DELAY TIMESTEP

#define SIMPORT "9930" // port we listen for OSC messages

#if 1
typedef float real;
#define MPI_MYREAL MPI::FLOAT
#else
typedef double real;
#define MPI_MYREAL MPI::DOUBLE
#endif

void error(int num, const char *msg, const char *path)
{
  fprintf(stderr, "oscin: liblo server error %d in path %s: %s\n", num, path, msg);
  exit(1);
}

typedef struct {
  lo_timetag timetag;
  real timeStamp;
  int msgCount;
  int blobsize;
  int datasize;
  real *data;
} keystate_t;

int keystate_handler(const char *path, const char *types, lo_arg ** argv,
		     int argc, void *data, void *user_data)
{
  keystate_t *keystate = (keystate_t*)user_data;
  
  keystate->timetag = lo_message_get_timestamp((lo_message)data);
  keystate->msgCount = argv[0]->i;
  keystate->timeStamp = argv[1]->d;
  keystate->blobsize = lo_blob_datasize((lo_blob)argv[2]);
  
  char *blobdata =  (char *)lo_blob_dataptr((lo_blob)argv[2]);
  real *dest = keystate->data;
  
  if (keystate->datasize != keystate->blobsize)
    std::runtime_error ("udpin: data size missmatch");
  
  for (int i = 0; i <  keystate->blobsize; i++)
    dest[i] = blobdata[i];
  
  return 1;
}


int main (int argc, char* argv[]) {
  
  MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);
  
  int width = atoi (argv[1]); // command line arg gives width

  MUSIC::ContOutputPort* keydata = setup->publishContOutput ("out");

  MPI::Intracomm comm = setup->communicator ();
  int nProcesses = comm.Get_size (); // how many processes are there?
  if (nProcesses > 1)
    {
      std::cout << "udpin: needs to run in a single MPI process\n";
      exit (1);
    }

  real* data = new real[width];
  for (int i = 0; i < width; ++i)
    data[i] = 0.0;
    
  // Declare what data we have to export
  MUSIC::ArrayData dmap (data, MPI_MYREAL, 0, width);
  keydata->map (&dmap, 1); // buffer only one tick
  
  double stoptime;
  setup->config ("stoptime", &stoptime);

  lo_server server = lo_server_new(SIMPORT, error);
  keystate_t keystate;
  keystate.data = data;
  keystate.datasize = width;
  lo_server_add_method(server, "/keystate", "idb", keystate_handler, &keystate);    

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, TIMESTEP);

  // Simulation loop
  for (; runtime->time () < stoptime; runtime->tick ()) {
    do {
      if (lo_server_recv(server) == -1)
	std::runtime_error ("udpin: lo_server_recv()");
    } while (keystate.timeStamp + DELAY < runtime->time ());    
  }
  
  runtime->finalize ();

  delete runtime;

  return 0;
}
