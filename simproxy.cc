/*
 *  simproxy.cc
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

#include <music.hh>
#include <cstring>

#include "OurUDPProtocol.hh"

struct OurUDPProtocol::toMusicPackage package;
struct OurUDPProtocol::fromMusicPackage outpackage;

std::array<double, OurUDPProtocol::COMMANDKEYS> commandKeyState;


int main (int argc, char* argv[]) {

  MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);

  MUSIC::ContInputPort * commands = setup->publishContInput ("commands");
  MUSIC::ContInputPort * in = setup->publishContInput ("sensorpool");
  MUSIC::ContOutputPort * out = setup->publishContOutput ("motorpool");

  MUSIC::ArrayData commandMap (&package.commandKeys,
			       MPI_DOUBLE,
			       0,
			       OurUDPProtocol::COMMANDKEYS);
  commands->map (&commandMap, 0.0, 1, false);

  MUSIC::ArrayData inMap (&package.keysPressed,
			  MPI_DOUBLE,
			  0,
			  OurUDPProtocol::KEYBOARDSIZE);
  in->map (&inMap, 0.0, 1, false);

  MUSIC::ArrayData outMap (&outpackage.keysPressed,
			   MPI_DOUBLE,
			   0,
			   OurUDPProtocol::KEYBOARDSIZE);
  out->map (&outMap, 1);
  
  double stoptime;
  if (!setup->config ("stoptime", &stoptime))
    throw std::runtime_error("simproxy: stoptime must be set in the MUSIC configuration file");

  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, OurUDPProtocol::TIMESTEP);

  for (; runtime->time () < stoptime; runtime->tick ()) {
    // Copy input to output
    std::copy(package.keysPressed.begin(), package.keysPressed.end(),
	      outpackage.keysPressed.begin());
    // Press another key, four halfnotes up
    std::transform(package.keysPressed.begin(), package.keysPressed.end()-4,
		   outpackage.keysPressed.begin()+4,
		   outpackage.keysPressed.begin()+4,
		   [] (double key, double newkey) { return (key>0.5) ? 1 : newkey; });

    // Print out if any command keys have changed
    std::transform(package.commandKeys.begin(), package.commandKeys.end(),
		   commandKeyState.begin(),
		   commandKeyState.begin(),
		   [] (double &key, double state) {
		     if (key != state)
		       std::cout << "commandkey "
				 << &key-package.commandKeys.begin()
				 << "=" << key << std::endl;
		     return key;
		   });
  }

  runtime->finalize ();

  delete runtime;

  return 0;
}
