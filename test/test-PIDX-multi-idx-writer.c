/*****************************************************
 **  PIDX Parallel I/O Library                      **
 **  Copyright (c) 2010-2014 University of Utah     **
 **  Scientific Computing and Imaging Institute     **
 **  72 S Central Campus Drive, Room 3750           **
 **  Salt Lake City, UT 84112                       **
 **                                                 **
 **  PIDX is licensed under the Creative Commons    **
 **  Attribution-NonCommercial-NoDerivatives 4.0    **
 **  International License. See LICENSE.md.         **
 **                                                 **
 **  For information about this project see:        **
 **  http://www.cedmav.com/pidx                     **
 **  or contact: pascucci@sci.utah.edu              **
 **  For support: PIDX-support@visus.net            **
 **                                                 **
 *****************************************************/

#include "pidxtest.h"

int test_multi_idx_writer(struct Args args, int rank, int nprocs)
{
#if PIDX_HAVE_MPI
  int i = 0, j = 0, k = 0;
  int ts, var, spv;
  int slice;
  int sub_div[3], local_offset[3];

  
  PIDX_file file;                                                // IDX file descriptor
  const char *output_file;                                                      // IDX File Name
  
  PIDX_variable* variable;                                       // variable descriptor
  unsigned long long     **long_data;
  double     **double_data;
  int* values_per_sample;
  
  //The command line arguments are shared by all processes
  MPI_Bcast(args.extents, 5, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(args.count_local, 5, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.debug_rst, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.debug_hz, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.time_step, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.idx_count, 3, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.blocks_per_file, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.bits_per_block, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.aggregation_factor, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.variable_count, 1, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast(&args.output_file_template, 512, MPI_CHAR, 0, MPI_COMM_WORLD);
  
  variable = malloc(sizeof(*variable) * args.variable_count);
  memset(variable, 0, sizeof(*variable) * args.variable_count);
  
  values_per_sample = malloc(sizeof(*values_per_sample) * args.variable_count);
  memset(values_per_sample, 0, sizeof(*values_per_sample) * args.variable_count);    
  
  
  //   Creating the filename 
  args.output_file_name = (char*) malloc(sizeof (char) * 512);
  sprintf(args.output_file_name, "%s%s", args.output_file_template,".idx");
  output_file = args.output_file_name;
  
  PIDX_point global_bounding_box, local_offset_point, local_box_count_point;
  //PIDX_create_point(&global_bounding_box);
  //PIDX_create_point(&local_offset_point);
  //PIDX_create_point(&local_box_count_point);
  
  
  //   Calculating every process's offset and count  
  sub_div[0] = (args.extents[0] / args.count_local[0]);
  sub_div[1] = (args.extents[1] / args.count_local[1]);
  sub_div[2] = (args.extents[2] / args.count_local[2]);
  local_offset[2] = (rank / (sub_div[0] * sub_div[1])) * args.count_local[2];
  slice = rank % (sub_div[0] * sub_div[1]);
  local_offset[1] = (slice / sub_div[0]) * args.count_local[1];
  local_offset[0] = (slice % sub_div[0]) * args.count_local[0];
  
  //int rank_x, rank_y, rank_z, rank_slice;
  //rank_z = rank / (sub_div[0] * sub_div[1]);
  //rank_slice = rank % (sub_div[0] * sub_div[1]);
  //rank_y = (rank_slice / sub_div[0]);
  //rank_x = (rank_slice % sub_div[0]);
  
  //printf("Rank %d broken into %d %d %d\n", rank, rank_x, rank_y, rank_z);
  
  PIDX_set_point_5D((long long)args.extents[0], (long long)args.extents[1], (long long)args.extents[2], 1, 1, global_bounding_box);
  PIDX_set_point_5D((long long)local_offset[0], (long long)local_offset[1], (long long)local_offset[2], 0, 0, local_offset_point);
  PIDX_set_point_5D((long long)args.count_local[0], (long long)args.count_local[1], (long long)args.count_local[2], 1, 1, local_box_count_point);
  
  PIDX_enable_time_step_caching_ON();
  for (ts = 0; ts < args.time_step; ts++) 
  {
#if long_data
    long_data = malloc(sizeof(*long_data) * args.variable_count);
    memset(long_data, 0, sizeof(*long_data) * args.variable_count);
#else
    double_data = malloc(sizeof(*double_data) * args.variable_count);
    memset(double_data, 0, sizeof(*double_data) * args.variable_count);
#endif
    
    PIDX_access access;
    PIDX_create_access(&access);

#if PIDX_HAVE_MPI
    PIDX_set_mpi_access(access, args.idx_count[0], args.idx_count[1], args.idx_count[2], MPI_COMM_WORLD);
    PIDX_set_process_extent(access, sub_div[0], sub_div[1], sub_div[2]);
#else
    PIDX_set_default_access(access);
#endif
    
    PIDX_file_create(output_file, PIDX_file_trunc, access, &file);
    PIDX_set_dims(file, global_bounding_box);
    PIDX_set_current_time_step(file, ts);
    PIDX_set_block_size(file, args.bits_per_block);
    PIDX_set_aggregation_factor(file, args.aggregation_factor);
    PIDX_set_block_count(file, args.blocks_per_file);
    PIDX_set_variable_count(file, args.variable_count);
    
    PIDX_debug_rst(file, args.debug_rst);
    PIDX_debug_hz(file, args.debug_hz);
    
    char variable_name[512];
    char data_type[512];
    
#if 0
    /// IO with Flush (memory efficient)
    for(var = 0; var < args.variable_count; var++)
    {
      values_per_sample[var] = var + 1;
      long_data[var] = malloc(sizeof (unsigned long long) * args.count_local[0] * args.count_local[1] * args.count_local[2]  * values_per_sample[var]);
      
      if(var % 2 == 0)
      {
        for (k = 0; k < args.count_local[2]; k++)
          for (j = 0; j < args.count_local[1]; j++)
            for (i = 0; i < args.count_local[0]; i++) 
            {
              long long index = (long long) (args.count_local[0] * args.count_local[1] * k) + (args.count_local[0] * j) + i;
              for (spv = 0; spv < values_per_sample[var]; spv++)
                long_data[var][index * values_per_sample[var] + spv] = 100 + ((args.extents[0] * args.extents[1]*(local_offset[2] + k))+(args.extents[0]*(local_offset[1] + j)) + (local_offset[0] + i));
            }
      }
      else
      {
        for (k = 0; k < args.count_local[2]; k++)
          for (j = 0; j < args.count_local[1]; j++)
            for (i = 0; i < args.count_local[0]; i++) 
            {
              long long index = (long long) (args.count_local[0] * args.count_local[1] * k) + (args.count_local[0] * j) + i;
              for (spv = 0; spv < values_per_sample[var]; spv++)
                long_data[var][index * values_per_sample[var] + spv] = (rank + 1);
            }
      }
      
      sprintf(variable_name, "variable_%d", var);
      sprintf(data_type, "%d*double64", values_per_sample[var]);
      PIDX_variable_create(file, variable_name, values_per_sample[var] * sizeof(unsigned long long) * 8, data_type, &variable[var]);
      PIDX_append_and_write_variable(variable[var], local_offset_point, local_box_count_point, long_data[var], PIDX_row_major);
      PIDX_flush(file);
      
      free(long_data[var]);
      long_data[var] = 0;
    }
    
    PIDX_close(file);
    PIDX_close_access(access);
    
    free(long_data);
    long_data = 0;
#endif

#if 1
    //printf("[%d] offset %d %d %d count %d %d %d %d %d\n", rank, local_offset[0], local_offset[1], local_offset[2], args.count_local[0], args.count_local[1], args.count_local[2], args.count_local[3], args.count_local[4]);
    /// IO with no Flush (high performance)
    
#if long_data
    for(var = 0; var < args.variable_count; var++)
    {
      values_per_sample[var] =  1;
      long_data[var] = malloc(sizeof (unsigned long long) * args.count_local[0] * args.count_local[1] * args.count_local[2]  * values_per_sample[var]);
      
      for (k = 0; k < args.count_local[2]; k++)
        for (j = 0; j < args.count_local[1]; j++)
          for (i = 0; i < args.count_local[0]; i++)
          {
            long long index = (long long) (args.count_local[0] * args.count_local[1] * k) + (args.count_local[0] * j) + i;
            for (spv = 0; spv < values_per_sample[var]; spv++)
              long_data[var][index * values_per_sample[var] + spv] = 100 + var + ((args.extents[0] * args.extents[1]*(local_offset[2] + k))+(args.extents[0]*(local_offset[1] + j)) + (local_offset[0] + i));
          }
    }
#else
    for(var = 0; var < args.variable_count; var++)
    {
      values_per_sample[var] =  1;
      double_data[var] = malloc(sizeof (unsigned long long) * args.count_local[0] * args.count_local[1] * args.count_local[2]  * values_per_sample[var]);
      
      for (k = 0; k < args.count_local[2]; k++)
        for (j = 0; j < args.count_local[1]; j++)
          for (i = 0; i < args.count_local[0]; i++)
          {
            long long index = (long long) (args.count_local[0] * args.count_local[1] * k) + (args.count_local[0] * j) + i;
            for (spv = 0; spv < values_per_sample[var]; spv++)
              //double_data[var][index * values_per_sample[var] + spv] = 100 + var + ((args.count_local[0] * args.count_local[1]*(k))+(args.count_local[0]*(j)) + (i));
              double_data[var][index * values_per_sample[var] + spv] = 100 + var + ((args.extents[0] * args.extents[1]*(local_offset[2] + k))+(args.extents[0]*(local_offset[1] + j)) + (local_offset[0] + i));
          }
    }
#endif
    
    for(var = 0; var < args.variable_count; var++)
    {
      sprintf(variable_name, "variable_%d", var);
      sprintf(data_type, "%d*float64", values_per_sample[var]);
      PIDX_variable_create(file, variable_name, values_per_sample[var] * sizeof(unsigned long long) * 8, data_type, &variable[var]);
#if long_data
      PIDX_append_and_write_variable(variable[var], local_offset_point, local_box_count_point, long_data[var], PIDX_row_major);
#else
      PIDX_append_and_write_variable(variable[var], local_offset_point, local_box_count_point, double_data[var], PIDX_row_major);
#endif
      
    }
    
    PIDX_close(file);
    PIDX_close_access(access);
    
#if long_data
    for(var = 0; var < args.variable_count; var++)
    {
      free(long_data[var]);
      long_data[var] = 0;
    }
#else
    for(var = 0; var < args.variable_count; var++)
    {
      free(double_data[var]);
      double_data[var] = 0;
    }
#endif

#endif

#if long_data
    free(long_data);
    long_data = 0;
#else
    free(double_data);
    double_data = 0;
#endif
    
  }
  PIDX_enable_time_step_caching_OFF();
  
  free(variable);
  free(values_per_sample);
  free(args.output_file_name);
  
#endif
  return 0;
}

/*   prints usage instructions   */
void usage_multi_idx_writer(void) 
{
  printf("Usage: test-multi-idx-PIDX-writer -g 4x4x4 -l 2x2x2 -f Filename_ -t 4\n");
  printf("  -g: global dimensions\n");
  printf("  -l: local (per-process) dimensions\n");
  printf("  -f: IDX Filename\n");
  printf("  -t: number of timesteps\n");
  printf("\n");
  return;
}