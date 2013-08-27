#include "BrownianForce.h"

#define GRANDOM ( r_gen.randNorm(0., 1.) ) // RNG gaussian with mean 0. and variance 1.

using namespace std;

BrownianForce::BrownianForce(System *sys_){
	sys = sys_;
	kb_T = sys->get_kb_T();
	kb_T2= 2*kb_T;

	forces = new double [3*sys->get_np()];

	ran_vector = new double [3*sys->get_np()];

}

BrownianForce::~BrownianForce(){

  delete [] forces;
  delete [] ran_vector;

}

void
BrownianForce::init(){
}


double*
BrownianForce::generate_invLFb(){
  int n3 = 3*sys->get_np();
  double sqrt_kbT2_dt = sqrt(kb_T2/sys->get_dt());
  for(int i=0; i<n3; i++){
	ran_vector[i] = sqrt_kbT2_dt * GRANDOM;
  }
		  
  return ran_vector;
}



