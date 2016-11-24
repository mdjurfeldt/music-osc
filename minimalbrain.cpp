/***********************************************************************************
   Created: 2016-09-28
   Modified: 2016-11-24
   Authors: Anders Lansner, Örjan Ekeberg
************************************************************************************/

#include <vector>
#include <string>
#include <mpi.h>

#include "base/source/Pop.h"
#include "base/source/Prj.h"
#include "base/source/Logger.h"
#include "utils/Utils.h"
#include "utils/RndGen.h"
#include "utils/Parseparam.h"
#include "utils/Patio.h"

#include "config.h"
#include "base/source/Globals.h"
#include "base/source/SensorPop.h" 
#include "base/source/PainPop.h" 
#include "base/source/RewardPop.h"
#include "base/source/MotorPop.h" 


using namespace std;

float reward;
float reset;

int main(int argc, char** argv) {
   
  Globals::init(argc,argv);
  Globals::setdt(0.001);

  int idur = 10;
    
  SensorPop* sensorpop = new SensorPop("sensorpool", 0.010, 1, 1, 88, 0.010, 0.0); 
  MotorPop* motorpop = new MotorPop("motorpool", 0.010, 1, 1, 88, 0.010, 0.0); 
    
  std::cerr << "After pop" << std::endl;

  Globals::start();

  std::cerr << "After start()" << std::endl;

  MPI_Barrier(Globals::_comm_world);

  if (ISROOT)
    std::cerr << "music sim time = " << Globals::_musicstoptime << std::endl;

  for (int simt=0; Globals::_musicruntime->time() < Globals::_musicstoptime; simt++) {
    for (int step=0; step<idur; step++) Globals::updstateall();
  }

  if (Globals::isroot()) printf(" Done!\n");

  Globals::stop();
    
  MPI_Barrier(Globals::_comm_world);
    
  Globals::report();
    
  Globals::finish();

  return 0;
}
