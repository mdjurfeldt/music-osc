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
#include <rtmidi/RtMidi.h>

#define VECTOR_DIM 88
#define PORT 9930

#if 0
typedef float real;
#else
typedef double real;
#endif

RtMidiOut *midiout;
std::vector<unsigned char> message;

void osc_key_pressed (real timeStamp, int id)
{
  // New key press
  message[0] = 144; // Note on
  message[1] = id + 21; // Key number
  message[2] = 90; // Velocity
  midiout->sendMessage( &message );
}

void
osc_key_released (real timeStamp, int id)
{
  // New key release
  message[0] = 128; // Note off
  message[1] = id + 21; // Key number
  message[2] = 40; // Velocity
  midiout->sendMessage( &message );
}


void init_midi(void) {
  midiout = new RtMidiOut();

  // Check available ports.
  unsigned int nPorts = midiout->getPortCount();
  if ( nPorts == 0 ) {
    throw std::runtime_error( "No MIDI ports available!");
  }

  // Open first available port.
  midiout->openPort( 0 );

  // Wait for the user to confirm
  std::cout << "Press return to start";
  while (std::cin.get() != '\n')
    ;
  
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
  struct sockaddr_in si_me, si_other;
  int s;
  unsigned int slen = sizeof (si_other);

  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("udptoosc: couldn't create socket");

  memset ((char *) &si_me, 0, sizeof (si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(PORT);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (s, (struct sockaddr *) &si_me, sizeof (si_me)) == -1)
    throw std::runtime_error ("udptoosc: could bind socket to port");

  real buf[1 + VECTOR_DIM];
  real state[VECTOR_DIM];
  std::for_each (std::begin (state), std::end (state),
		 [] (real &x) { x = 0.0; });
  
  init_midi();

  // Wait for start message
  if (recvfrom (s, buf, 2 * sizeof (real), 0, (struct sockaddr *) &si_other, &slen) == -1)
    throw std::runtime_error ("udptoosc: recvfrom()");
  else if (buf[0] != 4711.0)
    throw std::runtime_error ("udptoosc: bogus start message");
  real stoptime = buf[1];

  constexpr int buflen = sizeof (buf);
  buf[0] = 0.0;
  while (buf[0] < stoptime)
    {
      if (recvfrom(s, buf, buflen, 0, (struct sockaddr *) &si_other, &slen)
	  == -1)
	throw std::runtime_error ("udptoosc: failed to receive packet");
      std::cerr << ".";
      for (int i = 0; i < VECTOR_DIM; ++i)
	if (buf[i + 1] != state[i])
	  {
	    real timeStamp = buf[0];
	    if (buf[i + 1] != 0.0)
	      osc_key_pressed (timeStamp, i+1);
	    else
	      osc_key_released (timeStamp, i+1);
	    state[i] = buf[i + 1];
	  }
    }

  close(s);
  delete midiout;
  return 0;
}