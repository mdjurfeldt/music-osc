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

// Variables for the MIDI connection
RtMidiIn *midiin;
std::vector<unsigned char> message;


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

  // Don't ignore sysex, timing, or active sensing messages.
  midiin->ignoreTypes( false, false, false );
}


void
osc_events (real* state, bool* stop)
{
  double timeStamp;
  int nBytes;

  while (!*stop) {
    timeStamp = midiin->getMessage( &message );
    nBytes = message.size();

    if (nBytes > 2) {
      if (message[0] == 144 && message[2] != 0) {
	// Key press
	int id = message[1] - 21;
	if (id >= 0 && id < VECTOR_DIM)
	  state[id] = 1.0;
      } else
	if ((message[0] == 144 && message[2] == 0) || (message[0] == 128)) {
	  // Key release
	  int id = message[1] - 21;
	  if (id >= 0 && id < VECTOR_DIM)
	    state[id] = 0.0;
	}
    }
  }
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

  real buf[1 + VECTOR_DIM];

  // Wait for start message
  if (recvfrom (s, buf, 2 * sizeof (real), 0, (struct sockaddr *) &si_other, &slen) == -1)
    throw std::runtime_error ("miditoudp: recvfrom()");
  else if (buf[0] != 4712.0)
    throw std::runtime_error ("miditoudp: bogus start message");
  real stoptime = buf[1];

  std::for_each (std::begin (buf), std::end (buf),
		 [] (real &x) { x = 0.0; });

  bool stop = false;
  std::thread oscThread {osc_events, &buf[1], &stop};
  
  RTClock clock (TIMESTEP);
  constexpr int buflen = sizeof (buf);
  real t = 0.0;
  while (t < stoptime)
    {
      buf[0] = t;
      if (sendto (s, buf, buflen, 0, (struct sockaddr *) &si_other, slen) == -1)
	throw std::runtime_error ("miditoudp: sendto()");
      t += TIMESTEP;
      std::cerr << "-";
      clock.sleepNext ();
    }

  stop = true;
  oscThread.join ();

  close(s);
  return 0;
}
