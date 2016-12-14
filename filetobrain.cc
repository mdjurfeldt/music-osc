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

#include "OurUDPProtocol.hh"


MPI::Intracomm comm;
struct OurUDPProtocol::toMusicPackage package;


int
main (int argc, char* argv[])
{
  MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);

  if ((argc < 2) or (argc > 3)) {
    std::cerr << "Usage: filetobrain <musicfile>" << std::endl
	      << "or:    filetobrain <musicfile> <commandfile>" << std::endl;
    exit(1);
  }

  std::ifstream keyfile;
  keyfile.open(argv[1]);
  if (!keyfile.is_open()) {
    std::cerr << "Can not open file " << argv[1] << " for reading." << std::endl;
    exit(1);
  }

  
  std::ifstream commandfile;
  bool usecommandfile = false;

  if (argc >= 3) {
    usecommandfile = true;
    commandfile.open(argv[2]);
    if (!commandfile.is_open()) {
      std::cerr << "Can not open file " << argv[2] << " for reading." << std::endl;
      exit(1);
    }
  }

  
  comm = setup->communicator ();

  if (comm.Get_size() != 1) {
    std::cerr << "filetobrain must run on a single processor" << std::endl;
    exit(1);
  }

  // Describe the out port
  MUSIC::ContOutputPort* theport =
    setup->publishContOutput ("out");

  MUSIC::ArrayData dmap (&package.keysPressed,
			 MPI_DOUBLE,
			 0,
			 OurUDPProtocol::KEYBOARDSIZE);
  theport->map (&dmap);

  MUSIC::ContOutputPort* commandport;
  if (usecommandfile) {
    commandport = setup->publishContOutput ("commands");

    MUSIC::ArrayData dmap (&package.commandKeys,
			   MPI_DOUBLE,
			   0,
			   OurUDPProtocol::COMMANDKEYS);
    commandport->map (&dmap);
  }


  double stoptime;
  if (!setup->config ("stoptime", &stoptime))
    throw std::runtime_error("filetobrain: stoptime must be set from the music configuration file");

  double nextCommandTime;
  double infiniteFuture = 
    std::numeric_limits<double>::has_infinity ?
    std::numeric_limits<double>::infinity() :
    std::numeric_limits<double>::max();

  if (usecommandfile)
    commandfile >> nextCommandTime;
  
  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, OurUDPProtocol::TIMESTEP);

  std::cout << "Starting to feed file" << std::endl;
  for (; runtime->time () < stoptime; runtime->tick ()) {
    for (int i=0; i<OurUDPProtocol::KEYBOARDSIZE; i++) {
      keyfile >> package.keysPressed[i];
    }

    if (usecommandfile and (runtime->time() >= nextCommandTime)) {
      for (int i=0; i<OurUDPProtocol::COMMANDKEYS; ++i)
	commandfile >> package.commandKeys[i];
      std::cerr << "Command at " << nextCommandTime << std::endl;
      commandfile >> nextCommandTime;
      if (commandfile.eof())
	nextCommandTime = infiniteFuture;
    }

    usleep(10000);
  }

  runtime->finalize ();
  delete runtime;

  keyfile.close();

  return 0;
}
