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

#define VECTOR_DIM 10
#define PORT 9930

#if 0
typedef float real;
#else
typedef double real;
#endif

void
osc_key_pressed (real timeStamp, int id)
{
  std::cout << "udptoosc: " << timeStamp << " KEY " << id << " PRESSED\n";
}

void
osc_key_released (real timeStamp, int id)
{
  std::cout << "udptoosc: " << timeStamp << " KEY " << id << " RELEASED\n";
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
      for (int i = 0; i < VECTOR_DIM; ++i)
	if (buf[i + 1] != state[i])
	  {
	    real timeStamp = buf[0];
	    if (buf[i + 1] != 0.0)
	      osc_key_pressed (timeStamp, i);
	    else
	      osc_key_released (timeStamp, i);
	    state[i] = buf[i + 1];
	  }
    }

  close(s);
  return 0;
}
