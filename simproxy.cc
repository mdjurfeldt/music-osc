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

#define TIMESTEP 0.001
#define VECTOR_DIM 10

#if 1
typedef float real;
#define MPI_MYREAL MPI::FLOAT
#else
typedef double real;
#define MPI_MYREAL MPI::DOUBLE
#endif

int
main (int argc, char* argv[])
{
  MUSIC::Setup* setup = new MUSIC::Setup (argc, argv);

  MUSIC::ContInputPort * in = setup->publishContInput ("in");
  MUSIC::ContOutputPort * out = setup->publishContOutput ("out");

  real inBuf[VECTOR_DIM];
  MUSIC::ArrayData inMap (inBuf,
			  MPI_MYREAL,
			  0,
			  VECTOR_DIM);
  in->map (&inMap, 0.0, 1, false);

  real outBuf[VECTOR_DIM];

  for (int i = 0; i < VECTOR_DIM; ++i)
    {
      inBuf[i] = 0.0;
      outBuf[i] = 0.0;
    }

  MUSIC::ArrayData outMap (outBuf,
			   MPI_MYREAL,
			   0,
			   VECTOR_DIM);
  out->map (&outMap, 1);
  
  double stoptime;
  setup->config ("stoptime", &stoptime);
  MUSIC::Runtime* runtime = new MUSIC::Runtime (setup, TIMESTEP);
  for (; runtime->time () < stoptime; runtime->tick ())
    {
      memcpy (outBuf, inBuf, sizeof (outBuf));
      for (int i = 0; i < VECTOR_DIM; ++i)
	if (inBuf[i] != 0.0)
	  // Press next key also
	  outBuf[(i + 1) % VECTOR_DIM] = 1.0;
    }

  runtime->finalize ();

  delete runtime;

  return 0;
}
