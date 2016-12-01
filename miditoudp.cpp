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

#define TIMESTEP 0.010
#define VECTOR_DIM 88
#define PORT 9931

#if 0
typedef float real;
#else
typedef double real;
#endif

// Handle for the MIDI connection
RtMidiIn *midiin;

// Buffer for the UDP packages
real buffer[1 + VECTOR_DIM];


// MIDI event callback function
//
void midiEvent (double timestamp, std::vector<unsigned char>* message, void* voidstate) {
  real *keyboard = (real*)voidstate;

  if (message->size() > 2) {
    if ((*message)[0] == 144 && (*message)[2] != 0) {
      // Key press
      int id = (*message)[1] - 21;
      if (id >= 0 && id < VECTOR_DIM)
	keyboard[id] = 1.0;
    } else
      if (((*message)[0] == 144 && (*message)[2] == 0) || ((*message)[0] == 128)) {
	// Key release
	int id = (*message)[1] - 21;
	if (id >= 0 && id < VECTOR_DIM)
	  keyboard[id] = 0.0;
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
  midiin->setCallback( midiEvent, &buffer[1] );
}


int
main (int argc, char* argv[])
{
  struct sockaddr_in si_me, si_other;
  int s;
  unsigned int slen = sizeof (si_other);

  init_midi();

  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("miditoudp: couldn't create socket");

  memset ((char *) &si_me, 0, sizeof (si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(PORT);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (s, (struct sockaddr *) &si_me, sizeof (si_me)) == -1)
    throw std::runtime_error ("miditoudp: could bind socket to port");

  // Wait for start message
  if (recvfrom (s, buffer, 2 * sizeof (real), 0, (struct sockaddr *) &si_other, &slen) == -1)
    throw std::runtime_error ("miditoudp: recvfrom()");
  else if (buffer[0] != 4712.0)
    throw std::runtime_error ("miditoudp: bogus start message");
  real stoptime = buffer[1];

  std::for_each (std::begin (buffer), std::end (buffer),
		 [] (real &x) { x = 0.0; });


  RTClock clock (TIMESTEP);
  constexpr int buflen = sizeof (buffer);
  real t = 0.0;

  while (t < stoptime) {
    buffer[0] = t;
    if (sendto (s, buffer, buflen, 0, (struct sockaddr *) &si_other, slen) == -1)
      throw std::runtime_error ("miditoudp: sendto()");
    t += TIMESTEP;
    // std::cerr << "-";
    clock.sleepNext ();
  }

  close(s);
  return 0;
}
