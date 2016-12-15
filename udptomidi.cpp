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
#include <RtMidi.h>

#include "OurUDPProtocol.hh"

RtMidiOut *midiout;
std::vector<unsigned char> message;

struct OurUDPProtocol::fromMusicPackage buffer;
struct OurUDPProtocol::startPackage startBuffer;

std::array<bool, OurUDPProtocol::KEYBOARDSIZE> pressedState;



void midi_key_pressed (double timeStamp, int id)
{
  // New key press
  message[0] = 144; // Note on
  message[1] = id + 21; // Key number
  message[2] = 90; // Velocity
  midiout->sendMessage( &message );
}

void
midi_key_released (double timeStamp, int id)
{
  // New key release
  message[0] = 128; // Note off
  message[1] = id + 21; // Key number
  message[2] = 40; // Velocity
  midiout->sendMessage( &message );
}


void init_midi(void) {
  midiout = new RtMidiOut(RtMidi::UNSPECIFIED, "Milner Output");

  // Check available ports.
  unsigned int nPorts = midiout->getPortCount();
  if ( nPorts == 0 ) {
    throw std::runtime_error( "No MIDI ports available!");
  }

  // Search for a synth
  int selectedPort = 0;
  for (int i=0; i<nPorts; i++) {
    std::string s = midiout->getPortName(i);
    if (s.find("Synth") != std::string::npos) {
      selectedPort = i;
      std::cout << "udptomidi: Connecting to " << s << std::endl;
      break;
    }
  }
  if (selectedPort == 0)
    std::cout << "udptomidi: No Synth port found, using default MIDI output" << std::endl;
  midiout->openPort( selectedPort, "Milner Output" );

  // Send out a series of MIDI messages.
  // Program change: 192, 5
  message.push_back( 192 );
  message.push_back( 5 );
  midiout->sendMessage( &message );
  // Control Change: 176, 7, 100 (volume)
  message[0] = 176;
  message[1] = 7;
  message.push_back( 100 );
  midiout->sendMessage( &message );
}


int
main (int argc, char* argv[])
{
  int udpSocket;
  struct sockaddr_in udpAddressMe, udpAddress;
  unsigned int udpAddressSize = sizeof (udpAddress);

  if ((udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("udptomidi: couldn't create socket");

  memset ((char *) &udpAddressMe, 0, sizeof (udpAddressMe));
  udpAddressMe.sin_family = AF_INET;
  udpAddressMe.sin_port = htons(OurUDPProtocol::FROMMUSICPORT);
  udpAddressMe.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (udpSocket, (struct sockaddr *) &udpAddressMe, sizeof (udpAddressMe)) == -1)
    throw std::runtime_error ("udptomidi: could bind socket to port");

  init_midi();

  // Wait for start message
  if (recvfrom (udpSocket, &startBuffer, sizeof(startBuffer), 0, (struct sockaddr *) &udpAddress, &udpAddressSize) == -1)
    throw std::runtime_error ("udptomidi: recvfrom()");
  else if (startBuffer.magicNumber != OurUDPProtocol::MAGICFROMMUSIC)
    throw std::runtime_error ("udptomidi: bogus start message");
  double stoptime = startBuffer.stopTime;

  std::cout << "udptomidi: UDP-connection established." << std::endl;

  buffer.timestamp = 0.0;
  while (buffer.timestamp < stoptime)
    {
      if (recvfrom(udpSocket, &buffer, sizeof(buffer), 0, (struct sockaddr *) &udpAddress, &udpAddressSize)
	  == -1)
	throw std::runtime_error ("udptomidi: failed to receive packet");

      std::transform(buffer.keysPressed.begin(), buffer.keysPressed.end(),
		     pressedState.begin(),
		     pressedState.begin(),
		     [] (double &key, bool &state) {
		       if (key > 0.5) {
			 if (!state) // This was news
			   midi_key_pressed(buffer.timestamp, &key-buffer.keysPressed.begin());
			 return true;
		       } else {
			 if (state) // This was news
			   midi_key_released(buffer.timestamp, &key-buffer.keysPressed.begin());
			 return false;
		       }
		     });
    }

  close(udpSocket);
  delete midiout;
  return 0;
}
