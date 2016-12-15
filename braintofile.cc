/*
 *  braintofile.cc
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
#include <iostream>
#include <fstream>
#include <music.hh>

#include "OurUDPProtocol.hh"


MPI::Intracomm comm;

struct OurUDPProtocol::fromMusicPackage package;


int
main (int argc, char* argv[])
{
  MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);

  if (argc != 2) {
    std::cerr << "Usage: braintofile <filname>" << std::endl;
    exit(1);
  }

  std::ofstream keyfile;
  keyfile.open(argv[1]);
  if (!keyfile.is_open()) {
    std::cerr << "Can not open file " << argv[1] << " for writing." << std::endl;
    exit(1);
  }

  
  MUSIC::ContInputPort* theport =
    setup->publishContInput ("in");

  comm = setup->communicator ();

  if (comm.Get_size() != 1) {
    std::cerr << "braintofile must run on a single processor" << std::endl;
    exit(1);
  }

  // Declare what data we have to export
  MUSIC::ArrayData dmap (&package.keysPressed,
			 MPI_DOUBLE,
			 0,
			 OurUDPProtocol::KEYBOARDSIZE);
  theport->map (&dmap);
  
  double stoptime;
  if (!setup->config ("stoptime", &stoptime))
    throw std::runtime_error("braintofile: stoptime must be set from the music configuration file");


  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, OurUDPProtocol::TIMESTEP);

  for (; runtime->time () < stoptime; runtime->tick ()) {
    std::for_each(package.keysPressed.begin(), package.keysPressed.end()-1,
		  [&keyfile] (double key) { keyfile << key << " "; });
    keyfile << *package.keysPressed.end()-1 << std::endl;
  }

  runtime->finalize ();
  delete runtime;

  keyfile.close();

  return 0;
}
