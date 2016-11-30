/*
 *  filetobrain.cc
 *
 *  Copyright (C) 2016 Örjan Ekeberg <ekeberg@kth.se>
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  The software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <music.hh>

#define TIMESTEP 0.010
#define KEYBOARD_SIZE 88

// Use double for communication
typedef double real;
#define MPI_MYREAL MPI::DOUBLE

MPI::Intracomm comm;
real keyboard[KEYBOARD_SIZE];


int
main (int argc, char* argv[])
{
  MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);

  if (argc != 2) {
    std::cerr << "Usage: filetobrain <filname>" << std::endl;
    exit(1);
  }

  std::ifstream keyfile;
  keyfile.open(argv[1]);
  if (!keyfile.is_open()) {
    std::cerr << "Can not open file " << argv[1] << " for reading." << std::endl;
    exit(1);
  }

  
  MUSIC::ContOutputPort* theport =
    setup->publishContOutput ("out");

  comm = setup->communicator ();

  if (comm.Get_size() != 1) {
    std::cerr << "filetobrain must run on a single processor" << std::endl;
    exit(1);
  }

  // Declare what data we have to export
  MUSIC::ArrayData dmap (keyboard,
			 MPI_MYREAL,
			 0,
			 KEYBOARD_SIZE);
  theport->map (&dmap);
  
  double stoptime;
  setup->config ("stoptime", &stoptime);

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, TIMESTEP);

  std::cout << "Starting to feed file" << std::endl;
  for (; runtime->time () < stoptime; runtime->tick ()) {
    for (int i=0; i<KEYBOARD_SIZE; i++) {
      keyfile >> keyboard[i];
    }
    usleep(10000);
    std::cerr << "<";
  }

  runtime->finalize ();
  delete runtime;

  keyfile.close();

  return 0;
}
