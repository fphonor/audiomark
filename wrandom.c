#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "wrandom.h"
#define LARGE_DOUBLE 9999999.99

int *R_INTS = 0;
int R_INTS_SIZE = 0;
unsigned int random_seed = 0;

// seed all random number generators - TODO change so require a long?
unsigned seed_rand(unsigned seed){
  srand(seed);
	srand48((long)seed);
  return seed;
}

void free_rand(){
  free(R_INTS);
}

// sets an array of numbers to be randomized
void set_rand(unsigned seed, int size){
  if (seed > 0)
    seed_rand(seed);

  if(R_INTS)
    free_rand();

  R_INTS_SIZE = size;
  R_INTS = (int *)malloc(sizeof(int) * R_INTS_SIZE);
  for(int i = 0; i < R_INTS_SIZE; i++)
    R_INTS[i] = i;
}

// returns a random integer between 0 and R_INTS_SIZE, that hasn't been chosen
// in the past n_chosen calls.
int next_rand(int n_chosen) {
  if(R_INTS_SIZE - n_chosen <= 0){
    fprintf(stderr,"n_chosen:[%d] is greater than R_INTS_SIZE:[%d]!!\n", n_chosen, R_INTS_SIZE);
    return -1;
  }
  int i = rand() % (R_INTS_SIZE - n_chosen);

  int temp = R_INTS[i];
  R_INTS[i] = R_INTS[R_INTS_SIZE - n_chosen - 1];
  R_INTS[R_INTS_SIZE - n_chosen - 1] = temp;

  return temp;
}

// generates a random double from the uniform distribution [0,1)
double rand_double() {
  return drand48();
}

// generate a random double from the normal distribution with mean 0, var 1
// uses the Box-Muller transform, which produces 2 random numbers at a time.
double norm_double() {
	static double Z2;
	static int Z2_def = 0;

	if(Z2_def){
		Z2_def = 0;
		return Z2;
	}

	// generate two new independent normally distributed numbers, Z1 and Z2, from
	// uniformly distributed numbers U1 and U2.  U1 and U2 must be from the
	// uniform distribution (0,1] for Z1 and Z2 to be from the standard
	// distribution with mean 0 varience 1.
	double U1 = rand_double();
	double U2 = rand_double();
	if(U1 == 0) U1 = 1;
	if(U2 == 0) U2 = 1;

	Z2_def = 1;
	Z2 = sqrt(-2 * log(U1))*sin(2 * M_PI * U2);
	// below would be Z1
	return sqrt(-2 * log(U1))*cos(2 * M_PI * U2);
}
