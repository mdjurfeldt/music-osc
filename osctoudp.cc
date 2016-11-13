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

#include "rtclock.h"

#define TIMESTEP 0.001
#define VECTOR_DIM 10
#define PORT 9931

#if 1
typedef float real;
#else
typedef double real;
#endif

void
osc_events (real* state, bool* stop)
{
  while (!*stop)
    {
      // Sleep between 1 and 2 seconds
      usleep (1000000 + static_cast<int> (1e6 * drand48 ()));
      // Select key
      int id = random () % VECTOR_DIM;
      // Press
      state[id] = 1.0;
      usleep (500000);
      // Release
      state[id] = 0.0;
    }
}

int
main (int argc, char* argv[])
{
  struct sockaddr_in si_me, si_other;
  int s;
  unsigned int slen = sizeof (si_other);

  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    throw std::runtime_error ("osctoudp: couldn't create socket");

  memset ((char *) &si_me, 0, sizeof (si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(PORT);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind (s, (struct sockaddr *) &si_me, sizeof (si_me)) == -1)
    throw std::runtime_error ("osctoudp: could bind socket to port");

  real buf[1 + VECTOR_DIM];

  // Wait for start message
  if (recvfrom (s, buf, 2 * sizeof (real), 0, (struct sockaddr *) &si_other, &slen) == -1)
    throw std::runtime_error ("osctoudp: recvfrom()");
  else if (buf[0] != 4712.0)
    throw std::runtime_error ("osctoudp: bogus start message");
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
	throw std::runtime_error ("osctoudp: sendto()");
      t += TIMESTEP;
      clock.sleepNext ();
    }

  stop = true;
  oscThread.join ();

  close(s);
  return 0;
}
