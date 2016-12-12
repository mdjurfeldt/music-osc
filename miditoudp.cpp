#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <thread>
#include <rtmidi/RtMidi.h>

#include "rtclock.h"
#include "OurUDPProtocol.hh"


// Handle for the MIDI connection
RtMidiIn *midiin;

// Buffer for the UDP packages
struct OurUDPProtocol::toMusicPackage buffer;


// MIDI event callback function
//
void midiEvent (double timestamp, std::vector<unsigned char>* message, void* voidstate) {
  struct OurUDPProtocol::toMusicPackage *package = (struct OurUDPProtocol::toMusicPackage *)voidstate;

  if (message->size() > 2) {
    switch ((*message)[0]) {
    case 144: // Key press
      {
	int id = (*message)[1] - 21;
	if (id >= 0 && id < OurUDPProtocol::KEYBOARDSIZE)
	  package->keysPressed[id] = (*message)[2] > 0 ? 1.0 : 0.0; // Velocity=0 is interpreted as a key release
      }
      break;

    case 128: // Key release
      {
	int id = (*message)[1] - 21;
	if (id >= 0 && id < OurUDPProtocol::KEYBOARDSIZE)
	  package->keysPressed[id] = 0.0;
      }
      break;

    case 176: // MIDI Control
      if ((*message)[1] == 1)
	// Mod key
	package->commandKeys[OurUDPProtocol::MODULATECOMMAND] = (*message)[2] > 0 ? 1.0 : 0.0;
      else if ((*message)[1] == 64)
	// Sustain pedal
	package->commandKeys[OurUDPProtocol::SUSTAINCOMMAND] = (*message)[2] > 0 ? 1.0 : 0.0;
      else if ((*message)[1] == 7)
	// Volume
	package->commandKeys[OurUDPProtocol::VOLUMECOMMAND] = (*message)[2] / 127.0;

      break;

    case 224: // Pitch bend
      if ((*message)[1] == 8192) {
	// No more pitch bend
	package->commandKeys[OurUDPProtocol::PITCHBENDDOWNCOMMAND] = 0.0;
	package->commandKeys[OurUDPProtocol::PITCHBENDUPCOMMAND] = 0.0;
      }
      else if ((*message)[1] < 8192)
	// Pitch down
	package->commandKeys[OurUDPProtocol::PITCHBENDDOWNCOMMAND] = 1.0;
      else
	// Pitch up
	package->commandKeys[OurUDPProtocol::PITCHBENDUPCOMMAND] = 1.0;
      break;
    }
  }
}


void init_midi(void) {
  midiin = new RtMidiIn(RtMidi::UNSPECIFIED, "Milner Input");

  // Check available ports.
  unsigned int nPorts = midiin->getPortCount();
  if ( nPorts == 0 ) {
    std::runtime_error("No ports available!");
  }

  // Search for a keyboard named Keystation
  int selectedPort = 0;
  for (int i=0; i<nPorts; i++) {
    std::string s = midiin->getPortName(i);
    if (s.find("Keystation") != std::string::npos) {
      selectedPort = i;
      std::cout << "miditoudp: connecting to " << s << std::endl;
      break;
    }
  }
  if (selectedPort == 0)
    std::cout << "miditoudp: No Keystation port found, using default MIDI input" << std::endl;
  midiin->openPort( selectedPort, "Milner Input" );

  // Install our MIDI event callback function
  midiin->setCallback( midiEvent, &buffer );
}


int
main (int argc, char* argv[])
{
  int udpSocket;
  struct sockaddr_in udpAddressMe, udpAddress;
  unsigned int udpAddressSize = sizeof (udpAddress);

  init_midi();

  if ((udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("miditoudp: couldn't create socket");

  memset ((char *) &udpAddressMe, 0, sizeof (udpAddressMe));
  udpAddressMe.sin_family = AF_INET;
  udpAddressMe.sin_port = htons(OurUDPProtocol::TOMUSICPORT);
  udpAddressMe.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (udpSocket, (struct sockaddr *) &udpAddressMe, sizeof (udpAddressMe)) == -1)
    throw std::runtime_error ("miditoudp: could bind socket to port");

  // Wait for start message
  OurUDPProtocol::startPackage startBuffer;

  if (recvfrom (udpSocket, &startBuffer, sizeof (startBuffer), 0, (struct sockaddr *) &udpAddress, &udpAddressSize) == -1)
    throw std::runtime_error ("miditoudp: recvfrom()");
  else if (startBuffer.magicNumber != OurUDPProtocol::MAGICTOMUSIC)
    throw std::runtime_error ("miditoudp: bogus start message");
  double stoptime = startBuffer.stopTime;

  std::for_each (std::begin (buffer.keysPressed), std::end (buffer.keysPressed),
		 [] (double &x) { x = 0.0; });


  RTClock clock (OurUDPProtocol::TIMESTEP);
  double t = 0.0;

  while (t < stoptime) {
    buffer.timestamp = t;
    if (sendto (udpSocket, &buffer, sizeof(buffer), 0, (struct sockaddr *) &udpAddress, udpAddressSize) == -1)
      throw std::runtime_error ("miditoudp: sendto()");
    t += OurUDPProtocol::TIMESTEP;
    clock.sleepNext ();
  }

  close(udpSocket);
  return 0;
}
