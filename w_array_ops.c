#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "w_array_ops.h"
#include "config.h"


void _print_int(void *elt) { printf("%d", *(int *)elt); }
void _print_double(void *elt) { printf("%5.3lf", *(double *)elt); }
void _print_double_stdout(void *elt){ printf("%5.3lf ", *(double *)elt); }
void _print_pow_dens(void *elt) { printf("%5.3lf", pow_elem(*(complex *)elt)); }
void _print_complex(void *elt) {
	complex *c = (complex *)elt;
	printf("%5.3lf + %5.3lfi", creal(*c),cimag(*c));
}

void print_array(char *array, int n, int eltsize, void (*print)(void *))
{
	printf("[");
	print(array);
	for(int i = 1; i < n; i++){
		printf(", ");
		print(&array[i*eltsize]);
	}
	printf("]\n");
}

void print_complex_array(complex *array, int n){
	print_array((char *)array, n, sizeof(complex), &_print_complex);
}

void print_d_array(double *array, int n){
	print_array((char *)array, n, sizeof(double), &_print_double);
}

void print_i_array(int *array, int n){
	print_array((char *)array, n, sizeof(int), &_print_int);
}

void print_pow_density(complex *array, int n){
	print_array((char *)array, n, sizeof(complex), &_print_pow_dens);
}

void print_r_complex_array(complex *array, int n){
	print_array((char *)array, n, sizeof(complex), &_print_double);
}

double pow_elem(complex elem){
  return sqrt(pow(creal(elem),2) + pow(cimag(elem), 2));
}

void swap_d(double *a, double *b){
	double tmp = *a;
	*a = *b;
	*b = tmp;
}

// divide double *array of size size by n
void array_div(int n, double *array, int size){
/*{{{*/
  for(int i = 0; i < size; i++)
    array[i] /= n;
/*}}}*/
}

void array_add(double *array, double *addition, int size){
/*{{{*/
  for(int i = 0; i < size; i++)
    array[i] += addition[i];
/*}}}*/
}

void complex_array_div(int n, complex *array, int size){
/*{{{*/
  for(int i = 0; i < size; i++)
    array[i] /= n;
/*}}}*/
}

void complex_array_add(complex *a, complex *addition, int len){
  for(int i = 0; i < len; i++)
    a[i] += addition[i];
}

void d_array_to_complex(complex *a, double *addition, int len){
  for(int i = 0; i < len; i++)
    a[i] += addition[i];
}

void dub_x_dub_e_dub(double *a, double *b, double *c, int len){
	for(int i = 0; i < len; i++)
		c[i] = a[i] * b[i];
}

// helper for get_n_biggest: returns the index of idxs such that
// buffer[idxs[i]] is minimized
int _get_min_from_idxs(int *idxs, complex *buffer, int *n) {
	if(idxs[0] == -1) return 0;

	int min = 0;
	int min_index = 0;
	double min_pow = pow_elem(buffer[idxs[0]]);
	for(int i = 1; i < *n; i++){
		if(idxs[i] == -1)
			return i;
		if(min_pow > pow_elem(buffer[idxs[i]])){
			min_pow = pow_elem(buffer[idxs[i]]);
			min = idxs[i];
			min_index = i;
		}
	}
	return min_index;
}

//
// Returns an array of the idxs of the n largest components in a complex
// array of syze n.
// - the size of an elem is determined by pow_elem
// - order n*len time - could probably be faster
// -CURRENTLY doesn't choose dc offset or nyquist emelents
//  TODO move this logic somewhere else, and stop using A_FREQ_LEN in this file
int *get_n_biggest(complex *buffer, int len, int *n) {
	int *idxs = (int *) malloc(*n * sizeof(int));
	for (int i = 0; i < *n; i++)
    idxs[i] = -1;

	double min_pow = -1;
	int min_index=0;
	for (int i = 0; i < len; i++) {
		if(i % A_FREQ_LEN == 0) continue; // don't ever look at dc offset
		if((i+1) % A_FREQ_LEN == 0) continue; // don't ever look at nyquiest freq
		if(pow_elem(buffer[i]) > min_pow){
			idxs[min_index] = i;
			min_index = _get_min_from_idxs(idxs, buffer, n);
			min_pow = (idxs[min_index] == -1) ? -1 : pow_elem(buffer[idxs[min_index]]);
		}
    // printf("%f, %f\n", pow_elem(buffer[i]), min_pow);
	}
  // int *ids = (int *) malloc(*n * sizeof(int));
  // int count_above_lower_limit = 0;
  // for (int i = 0; i < *n; i++) {
  //   if (pow_elem(buffer[idxs[i]]) >= LOWER_LIMIT_OF_POW) {
  //     ids[count_above_lower_limit] = idxs[i];
  //     count_above_lower_limit ++;
  //   }
  // }
  // if (DEBUG) {
  //   static int CCC = 0, CCL = 0;
  //   double *ds = (double *) malloc(*n * sizeof(double));
  //   complex *cs = (complex *) malloc(*n * sizeof(complex));

  //   for (int i = 0; i < *n; i++) {
  //     ds[i] = pow_elem(buffer[idxs[i]]);
  //     cs[i] = buffer[idxs[i]];
  //   }
  //   printf("ET: ");
  //   print_d_array(ds, *n);
  //   print_complex_array(cs, *n);
  //   CCC += *n;

  //   *n = count_above_lower_limit;
  //   idxs = ids;
  //   CCL += *n;
  //   ds = (double *) malloc(*n * sizeof(double));
  //   cs = (complex *) malloc(*n * sizeof(complex));

  //   for (int i = 0; i < *n; i++) {
  //     ds[i] = pow_elem(buffer[idxs[i]]);
  //     cs[i] = buffer[idxs[i]];
  //   }
  //   printf("- - - - - - - - - - - - - - - - - - - - %d\n", CCC);
  //   print_d_array(ds, *n);
  //   print_complex_array(cs, *n);
  //   printf("--------------------------------------- %d\n", CCL);
  // }
  // *n = count_above_lower_limit;
  // idxs = ids;
	return idxs;
}
