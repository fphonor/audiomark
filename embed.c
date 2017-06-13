#ifndef W_INCLUDES
#define W_INCLUDES

#include <complex.h>
#include <math.h>
#include <fftw3.h>
#include <sndfile.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "watermark.h"
#include "wrandom.h"
#include "w_array_ops.h"

#endif

// pulled from watermark.c
extern	watermark *wmark;
int	embed_debug = 14;
int	embed_iteration = 0;
int embed_verbose = 0;

// These functions are only used in embedding
void  print_embedding_info(char *infile_path, char *outfile_path, char *orig_out_path, char *config_path);
void  watermark_elt(double w_d, complex *freq_elt);
void  ss_embed_to_signal(complex *freq_buffer, int f_buffer_len);
void  fh_embed_to_signal(complex *freq_buffer, int f_buffer_len);
int   embed(char *infile_path, char *outfile_path, char *orig_outfile_path);

int main(int argc, char *argv[]){
  char		infile_path[PATH_SIZE];
  char		outfile_path[PATH_SIZE];
  char		config_path[PATH_SIZE];
  char		orig_out_path[PATH_SIZE];
  FILE		*config;

  // set default parameters for testing
  strcpy(infile_path, "audio/input.wav");
  strcpy(outfile_path, "audio/out.wav");
  strcpy(orig_out_path, "audio/orig_out.wav");
  strcpy(config_path, "wmark.cfg");

  //
  // Parse parameters
  //
  if(argc > 1)
    strncpy(infile_path, argv[1], PATH_SIZE);
  if(argc > 2)
    strncpy(outfile_path, argv[2], PATH_SIZE);
  if(argc > 3)
    strncpy(config_path, argv[3], PATH_SIZE);

	gen_default_wmark();
  if(ERROR == parse_config(config_path)){
    fprintf(stderr, "Error parsing config");
    goto main_freedom;
  }

	if(embed_verbose){
		print_embedding_info(infile_path, outfile_path, orig_out_path, config_path);
		print_watermark_info();
	}

  if(embed(infile_path, outfile_path, orig_out_path))
    goto main_freedom;

main_freedom:
  free(wmark->message);
  free(wmark);
} // main

void watermark_elt(double w_d, complex *freq_elt)
{ //{{{
	switch (wmark->schema) {
		case PLUS_SCHEMA:
			*freq_elt += wmark->alpha * w_d;
			*freq_elt += wmark->alpha * w_d * I;
			return;
		case MULT_SCHEMA:
			*freq_elt = *freq_elt * (1 + (wmark->alpha)*w_d);
			return;
		case POWR_SCHEMA:
			*freq_elt = *freq_elt * pow(M_E, wmark->alpha*w_d);
			return;
		default:
			return;
	}
} //}}}

void fh_embed_to_signal(complex *freq_buffer, int f_buffer_len)
{ //{{{
    if(embed_iteration >= 4 && embed_iteration <= 5 || embed_iteration == 0)
      printf("embed %d orig: ", embed_iteration); print_pow_density(freq_buffer, 10);

  char *message = wmark->message;
  int next_r = 0;
  for(int i = 0; i < wmark->len; i++){
    next_r = next_rand(i);
    complex old_freq_elt = freq_buffer[next_r];
    watermark_elt(c_to_d(message[i]), &(freq_buffer[next_r]));

    if(embed_iteration >= 4 && embed_iteration <= 5 || embed_iteration == 0){
      printf("embed %d %c, %1.4f+%1.4fi => %1.4f+%1.4fi : %d : %f\n",
	  embed_iteration, message[i],
	  creal(old_freq_elt), cimag(old_freq_elt),
	  creal(freq_buffer[next_r]), cimag(freq_buffer[next_r]),
	  next_r, c_to_d(message[i]));
    }
  }
} //}}}

// Watermark embedding process described in Cox et al.
void ss_embed_to_signal(complex *freq_buffer, int f_buffer_len)
{ //{{{
	// extract V, represented here by a set of indices from the
	// frequency domain.
	int *embed_indices;

	// avoid sending dc offset at potential index
	int noise_len = extract_sequence_indices(freq_buffer, f_buffer_len-1, 
																					 &embed_indices);
	
	if(embed_iteration == embed_debug){
		//printf("embed indices:\n");
		//print_i_array(embed_indices, 20);
	}
	// noise_seq corresponds to W
  double	noise_seq[noise_len];
  generate_noise(noise_seq, noise_len);
  embed_to_noise(noise_seq, noise_len);

	// embed W into V
  for(int i = 0; i < noise_len; i++){
		int freq_i = embed_indices[i];
    watermark_elt(noise_seq[i], &(freq_buffer[freq_i]));
	}

	free(embed_indices);
} //}}}

int embed(char *infile_path, char *outfile_path, char *orig_outfile_path)
{ //{{{
  SF_INFO	sfinfo;		// struct with info on the samplerate, number of channels, etc
  SNDFILE   	*s_infile;
  SNDFILE   	*s_outfile;
  SNDFILE			*s_orig_out;

  fftw_plan 	*ft_forward;
  fftw_plan 	*ft_reverse;
  double	*time_buffer;
  complex	*freq_buffer;

  void		(*embed_to_signal)(complex *, int);
	int channels;
  
  if(wmark->type == FH_EMBED)
    embed_to_signal = &fh_embed_to_signal;
  else
    embed_to_signal = &ss_embed_to_signal;

  set_rand(wmark->key_seed, wmark->len); 

  //
  // Open input file and output file
  // -------------------------------

  // The pre-open SF_INFO should have format = 0, everything else will be set in the open call
  sfinfo.format = 0;
  
  // s_infile = input audio file
  if(!(s_infile = sf_open(infile_path, SFM_READ, &sfinfo))){
    fprintf(stderr,"error opening the following file for reading: %s\n", infile_path);
    return 1;
  }

  // s_outfile = watermaked audio file
  if(!(s_outfile = sf_open(outfile_path, SFM_WRITE, &sfinfo))){
    fprintf(stderr,"error opening the following file for writing: %s\n", outfile_path);
    return 1;
  }

  // s_orig_out = watermaked audio file
  if(!(s_orig_out = sf_open(orig_outfile_path, SFM_WRITE, &sfinfo))){
    fprintf(stderr,"error opening the following file for writing: %s\n", orig_outfile_path);
    return 1;
  }

	channels = sfinfo.channels;
	if(embed_verbose)
		print_sfile_info(sfinfo);

  //
  // Read, process, and write data
  // -----------------------------

  time_buffer = (double *) malloc(sizeof(double) * BUFFER_LEN * channels);
  freq_buffer = (complex *) fftw_malloc(sizeof(complex) * FREQ_LEN * channels);
  
	ft_forward = (fftw_plan *)malloc(sizeof(fftw_plan) * sfinfo.channels);
	ft_reverse = (fftw_plan *)malloc(sizeof(fftw_plan) * sfinfo.channels);

	for(int i = 0; i < channels; i++){
		ft_forward[i] = fftw_plan_dft_r2c_1d(BUFFER_LEN, time_buffer+i*BUFFER_LEN,
																				 freq_buffer+i*FREQ_LEN, FFTW_ESTIMATE);
		ft_reverse[i] = fftw_plan_dft_c2r_1d(BUFFER_LEN, freq_buffer+i*FREQ_LEN,
																			 time_buffer+i*BUFFER_LEN, FFTW_ESTIMATE);
	}

  int bytes_read;

  int counter = 0;
  while(bytes_read = sf_read_double(s_infile, time_buffer, BUFFER_LEN*channels)){
		// embed to orig_out to account for some of the floating point errors
		sf_write_double(s_orig_out, time_buffer, bytes_read);
      
    // only embed in a frame where we read in the full amount for the DFT
		if(bytes_read == BUFFER_LEN*channels){

			// if there are, say, 2 channels a,b, the audio will be stored as
			// a1, b1, a2, b2, ...
			// convert it to a1, a2, ... , b1, b2, ...
			deinterleave_channels(time_buffer, channels);

			for(int i = 0; i < channels; i++)
				fftw_execute(ft_forward[i]);

			// DEBUG print embed read info
			if(counter == embed_debug){
				printf("org: ");
				print_pow_density(freq_buffer, 10);
			}

			embed_to_signal(freq_buffer, FREQ_LEN*channels);

			// DEBUG print embed read info
			if(counter == embed_debug){
				printf("new: ");
				print_pow_density(freq_buffer, 10);
			}

			for(int i = 0; i < channels; i++)
				fftw_execute(ft_reverse[i]);

			// The DFT naturally multiplies every array element by the size of the array, reverse this
			array_div(BUFFER_LEN, time_buffer, BUFFER_LEN*channels);

			interleave_channels(time_buffer, channels);

		} // if(bytes_read == BUFFER_LEN)
		sf_write_double(s_outfile, time_buffer, bytes_read);

			//if(counter == embed_debug){
			//	deinterleave_channels(time_buffer, channels);
			//	for(int i = 0; i < channels; i++)
			//		fftw_execute(ft_forward[i]);
			//		printf("pt:  "); print_pow_density(freq_buffer,10);
			//		printf("cpt: "); print_complex_array(freq_buffer,10);

			//	for(int i = 0; i < channels; i++)
			//		fftw_execute(ft_reverse[i]);
			//	interleave_channels(time_buffer, channels);
			//	array_div(BUFFER_LEN, time_buffer, BUFFER_LEN*channels);
			//}

		counter++;
		embed_iteration++;
	} //while

  //
  // free everything
  // ---------------

	for(int i = 0; i < channels; i++){
		fftw_destroy_plan(ft_forward[i]);
		fftw_destroy_plan(ft_reverse[i]);
	}
	free(ft_forward);
	free(ft_reverse);
  free(time_buffer);
  fftw_free(freq_buffer);
  free_rand();
  sf_close(s_infile);
  sf_close(s_outfile);
  sf_close(s_orig_out);
} //}}}

void print_embedding_info(char *infile_path, char *outfile_path, char *orig_out_path, char *config_path)
{ //{{{
  char buffer[1000];
  int l_to_wmark = strlen(infile_path) + strlen("transform") + 6;

  puts("");
  int i;
  // get line 1
  for(i = 0; i < l_to_wmark; i++)
    buffer[i] = ' ';
  strncpy(buffer + i, config_path, strlen(config_path));
  i += strlen(config_path);
  buffer[i] = '\0';
  puts(buffer);

  // line 2
  for(i = 0; i < l_to_wmark + 2; i++)
    buffer[i] = ' ';
  buffer[i] = '|';
  buffer[i+1] = '\0';
  puts(buffer);

  // line 3
  for(i = 0; i < l_to_wmark + 2; i++)
    buffer[i] = ' ';
  buffer[i] = 'v';
  buffer[i+1] = '\0';
  puts(buffer);

  // line 4
  strncpy(buffer, infile_path, strlen(infile_path));
  i = strlen(infile_path);

  strncpy(buffer + i, "-->", 3);
  i += 3;

  strncpy(buffer+i, "transform", strlen("transform"));
  i += strlen("transform");

  strncpy(buffer + i, "-->", 3);
  i += 3;
  
  strncpy(buffer + i, "embed", strlen("embed"));
  i += strlen("embed");

  strncpy(buffer + i, "-->", 3);
  i += 3;

  strncpy(buffer+i, "transform", strlen("transform"));
  i += strlen("transform");

  strncpy(buffer + i, "-->", 3);
  i += 3;

  strncpy(buffer + i, outfile_path, strlen(outfile_path)); 
  buffer[i + strlen(outfile_path)] = '\0';

  puts(buffer);

  // line 5
  for(i = 0; i < strlen(infile_path) + 8; i++)
    buffer[i] = ' ';
  buffer[i++] = '|';
  buffer[i] = '\0';
  puts(buffer);
  
  // line 6
  for(i = 0; i < strlen(infile_path) + 8; i++)
    buffer[i] = ' ';
  buffer[i++] = 'L';
  for(; i < l_to_wmark + strlen("embed") + 2; i++)
    buffer[i] = '-';
  buffer[i++] = '>';

  strncpy(buffer+i, "transform", strlen("transform"));
  i += strlen("transform");

  strncpy(buffer + i, "-->", 3);
  i += 3;

  strncpy(buffer + i, orig_out_path, strlen(orig_out_path));
  buffer[i + strlen(orig_out_path)] = '\0';

  puts(buffer);
  puts("");
} //}}}
