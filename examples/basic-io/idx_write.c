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

/*
  PIDX write example

  In this example we show how to write data using the PIDX library.

  We consider a global 3D regular grid domain that we will call 
  global domain (g).
  This global domain represents the grid space where all the data are stored.

  In a parallel environment each core (e.g. MPI rank) owns a portion of the data
  that has to be written on the disk. We refer to this portion of the domain as
  local domain (l).

  In this example we well see how to execute parallel write with PIDX of a 
  syntethic dataset.

  In the following picture is represented a sample domain decomposition
  of the global domain (l) in per-core local domains (l), sometimes referred
  as patches.
  In this example all the local domains have same dimesions for simplicity.
  PIDX supports different number and sizes of patches per core.

             *---------*--------*
           /         /         /| P7
          *---------*---------* |
         /         /         /| |
        *---------*---------* | *
        |         |         | |/|           --------->        IDX Data format
        |         |         | * |
        | P4      | P5      |/| | P3
        *---------*---------* | *
        |         |         | |/
        |         |         | *
        | P0      | P1      |/
        *---------*---------*

*/

#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <PIDX.h>

#define MAX_VAR_COUNT 256
enum { X, Y, Z, NUM_DIMS };

static int process_count = 1, rank = 0;
static unsigned long long rst_box_size[NUM_DIMS];
static unsigned long long global_box_size[NUM_DIMS];
static unsigned long long local_box_offset[NUM_DIMS];
static unsigned long long local_box_size[NUM_DIMS];
int sub_div[NUM_DIMS];
static int time_step_count = 1;
static int variable_count = 1;
static char output_file_template[512];
static char var_list[512];
static unsigned char **data;
static char output_file_name[512];
static char var_name[MAX_VAR_COUNT][512];
static int bpv[MAX_VAR_COUNT];
static char type_name[MAX_VAR_COUNT][512];
static int vps[MAX_VAR_COUNT];

static PIDX_point global_size, local_offset, local_size, reg_size;
static PIDX_access p_access;
static PIDX_file file;
static PIDX_variable* variable;

static void init_mpi(int argc, char **argv);
static void parse_args(int argc, char **argv);
static int parse_var_list();
static int generate_vars();
static void check_args();
static void calculate_per_process_offsets();
static void create_synthetic_simulation_data();
static void terminate_with_error_msg(const char *format, ...);
static void terminate();
static void set_pidx_file(int ts);
static void set_pidx_variable();
static void create_pidx_var_point_and_access();
static void destroy_pidx_var_point_and_access();
static void destroy_synthetic_simulation_data();
static void shutdown_mpi();

static char *usage = "Serial Usage: ./idx_write -g 32x32x32 -l 32x32x32 -v 2 -t 4 -f output_idx_file_name\n"
                     "Parallel Usage: mpirun -n 8 ./idx_write -g 64x64x64 -l 32x32x32 -v 2 -t 4 -f output_idx_file_name\n"
                     "  -g: global dimensions\n"
                     "  -l: local (per-process) dimensions\n"
                     "  -r: restructured box dimension\n"
                     "  -f: file name template (without .idx)\n"
                     "  -t: number of timesteps\n"
                     "  -v: number of variables (or file containing a list of variables)\n";

int main(int argc, char **argv)
{
  int ts = 0, var = 0;

  // Init MPI and MPI vars (e.g. rank and process_count)
  init_mpi(argc, argv);

  // Parse input arguments and initialize 
  // corresponing variables
  parse_args(argc, argv);

  // Verify that the domain decomposition is valid
  // for the given number of cores
  check_args();

  // Initialize per-process local domain
  calculate_per_process_offsets();

  // Generate synthetic data
  create_synthetic_simulation_data();

  // Create variables
  create_pidx_var_point_and_access();

  for (ts = 0; ts < time_step_count; ts++)
  {
    // Set PIDX_file for this timestep
    set_pidx_file(ts);
    
    // Set all the PIDX_variable that we want to write
    for (var = 0; var < variable_count; var++)
      set_pidx_variable(var);

    // PIDX_close triggers the actual write on the disk
    // of the variables that we just set
    PIDX_close(file);
  }

  // Clean up our mess
  destroy_pidx_var_point_and_access();

  destroy_synthetic_simulation_data();

  shutdown_mpi();

  return 0;
}

//----------------------------------------------------------------
static void init_mpi(int argc, char **argv)
{
#if PIDX_HAVE_MPI
  if (MPI_Init(&argc, &argv) != MPI_SUCCESS)
    terminate_with_error_msg("ERROR: MPI_Init error\n");
  if (MPI_Comm_size(MPI_COMM_WORLD, &process_count) != MPI_SUCCESS)
    terminate_with_error_msg("ERROR: MPI_Comm_size error\n");
  if (MPI_Comm_rank(MPI_COMM_WORLD, &rank) != MPI_SUCCESS)
    terminate_with_error_msg("ERROR: MPI_Comm_rank error\n");
#endif
}

int isNumber(char number[])
{
    int i = 0;

    //checking for negative numbers
    if (number[0] == '-')
        i = 1;
    for (; number[i] != 0; i++)
    {
        //if (number[i] > '9' || number[i] < '0')
        if (!isdigit(number[i]))
            return 0;
    }
    return 1;
}

int nextPow2(int v)
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;

  return v;
}

//----------------------------------------------------------------
static void parse_args(int argc, char **argv)
{
  char flags[] = "g:l:r:f:t:v:";
  int one_opt = 0;
  int with_rst = 0;

  while ((one_opt = getopt(argc, argv, flags)) != EOF)
  {
    /* postpone error checking for after while loop */
    switch (one_opt)
    {
    case('g'): // global dimension
      if ((sscanf(optarg, "%lldx%lldx%lld", &global_box_size[X], &global_box_size[Y], &global_box_size[Z]) == EOF) || (global_box_size[X] < 1 || global_box_size[Y] < 1 || global_box_size[Z] < 1))
        terminate_with_error_msg("Invalid global dimensions\n%s", usage);
      break;

    case('l'): // local dimension
      if ((sscanf(optarg, "%lldx%lldx%lld", &local_box_size[X], &local_box_size[Y], &local_box_size[Z]) == EOF) ||(local_box_size[X] < 1 || local_box_size[Y] < 1 || local_box_size[Z] < 1))
        terminate_with_error_msg("Invalid local dimension\n%s", usage);
      break;

    case('r'): // local dimension
      if ((sscanf(optarg, "%lldx%lldx%lld", &rst_box_size[X], &rst_box_size[Y], &rst_box_size[Z]) == EOF) ||(rst_box_size[X] < 1 || rst_box_size[Y] < 1 || rst_box_size[Z] < 1))
        terminate_with_error_msg("Invalid restructuring box dimension\n%s", usage);
      else
        with_rst = 1;
      break;

    case('f'): // output file name
      if (sprintf(output_file_template, "%s", optarg) < 0)
        terminate_with_error_msg("Invalid output file name template\n%s", usage);
      sprintf(output_file_name, "%s%s", output_file_template, ".idx");
      break;

    case('t'): // number of timesteps
      if (sscanf(optarg, "%d", &time_step_count) < 0)
        terminate_with_error_msg("Invalid variable file\n%s", usage);
      break;

    case('v'): // number of variables
      if(!isNumber(optarg)){ // the param is a file with the list of variables
        if (sprintf(var_list, "%s", optarg) > 0) 
          parse_var_list();
        else
          terminate_with_error_msg("Invalid variable list file\n%s", usage);
      }else { // the param is a number of variables (default: 1*float32)
        if(sscanf(optarg, "%d", &variable_count) > 0)
          generate_vars();
        else
          terminate_with_error_msg("Invalid number of variables\n%s", usage);
      }
      break;

    default:
      terminate_with_error_msg("Wrong arguments\n%s", usage);
    }
  }

  if(!with_rst){
    // Set default restructuring box size
    rst_box_size[X] = nextPow2(local_box_size[X]);
    rst_box_size[Y] = nextPow2(local_box_size[Y]);
    rst_box_size[Z] = nextPow2(local_box_size[Y]);
  }
}

static int generate_vars(){
  
  int variable_counter = 0;

  for(variable_counter = 0; variable_counter < variable_count; variable_counter++){
    int ret;
    int bits_per_sample = 0;
    int sample_count = 0;
    char temp_name[512];
    char* temp_type_name = "1*float64";
    sprintf(temp_name, "var_%d", variable_counter);
    strcpy(var_name[variable_counter], temp_name);
    strcpy(type_name[variable_counter], temp_type_name);

    ret = PIDX_values_per_datatype(temp_type_name, &sample_count, &bits_per_sample);
    if (ret != PIDX_success)  return PIDX_err_file;

    bpv[variable_counter] = bits_per_sample;
    vps[variable_counter] = sample_count;
  }

  return 0;
}

//----------------------------------------------------------------
static int parse_var_list()
{
  FILE *fp = fopen(var_list, "r");
  if (fp == NULL)
  {
    fprintf(stderr, "Error Opening %s\n", var_list);
    return PIDX_err_file;
  }

  int variable_counter = 0, count = 0, len = 0;
  char *pch1;
  char line [ 512 ];

  while (fgets(line, sizeof (line), fp) != NULL)
  {
    line[strcspn(line, "\r\n")] = 0;

    if (strcmp(line, "(fields)") == 0)
    {
      if( fgets(line, sizeof line, fp) == NULL)
        return PIDX_err_file;
      line[strcspn(line, "\r\n")] = 0;
      count = 0;
      variable_counter = 0;

      while (line[X] != '(')
      {
        pch1 = strtok(line, " +");
        while (pch1 != NULL)
        {
          if (count == 0)
          {
            char* temp_name = strdup(pch1);
            strcpy(var_name[variable_counter], temp_name);
            free(temp_name);
          }

          if (count == 1)
          {
            len = strlen(pch1) - 1;
            if (pch1[len] == '\n')
              pch1[len] = 0;

            strcpy(type_name[variable_counter], pch1);
            int ret;
            int bits_per_sample = 0;
            int sample_count = 0;
            ret = PIDX_values_per_datatype(type_name[variable_counter], &sample_count, &bits_per_sample);
            if (ret != PIDX_success)  return PIDX_err_file;

            bpv[variable_counter] = bits_per_sample;
            vps[variable_counter] = sample_count;
          }
          count++;
          pch1 = strtok(NULL, " +");
        }
        count = 0;

        if( fgets(line, sizeof line, fp) == NULL)
          return PIDX_err_file;
        line[strcspn(line, "\r\n")] = 0;
        variable_counter++;
      }
      variable_count = variable_counter;
    }
  }
  fclose(fp);

  /*
  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0)
  {
    int v = 0;
    for(v = 0; v < variable_count; v++)
      fprintf(stderr, "[%d] -> %s %d %d\n", v, var_name[v], bpv[v], vps[v]);
  }
  */

  return PIDX_success;
}

//----------------------------------------------------------------
static void check_args()
{
  if (global_box_size[X] < local_box_size[X] || global_box_size[Y] < local_box_size[Y] || global_box_size[Z] < local_box_size[Z])
    terminate_with_error_msg("ERROR: Global box is smaller than local box in one of the dimensions\n");

  // check if the number of processes given by the user is consistent with the actual number of processes needed
  int brick_count = (int)((global_box_size[X] + local_box_size[X] - 1) / local_box_size[X]) *
                    (int)((global_box_size[Y] + local_box_size[Y] - 1) / local_box_size[Y]) *
                    (int)((global_box_size[Z] + local_box_size[Z] - 1) / local_box_size[Z]);
  if(brick_count != process_count)
    terminate_with_error_msg("ERROR: Number of sub-blocks (%d) doesn't match number of processes (%d)\n", brick_count, process_count);
}

//----------------------------------------------------------------
static void calculate_per_process_offsets()
{
  sub_div[X] = (global_box_size[X] / local_box_size[X]);
  sub_div[Y] = (global_box_size[Y] / local_box_size[Y]);
  sub_div[Z] = (global_box_size[Z] / local_box_size[Z]);
  local_box_offset[Z] = (rank / (sub_div[X] * sub_div[Y])) * local_box_size[Z];
  int slice = rank % (sub_div[X] * sub_div[Y]);
  local_box_offset[Y] = (slice / sub_div[X]) * local_box_size[Y];
  local_box_offset[X] = (slice % sub_div[X]) * local_box_size[X];
}

//----------------------------------------------------------------
static void create_synthetic_simulation_data()
{
  int var = 0;
  data = malloc(sizeof(*data) * variable_count);
  memset(data, 0, sizeof(*data) * variable_count);

  // Synthetic simulation data
  for(var = 0; var < variable_count; var++)
  {
    //fprintf(stderr, "vps[var] %d - bpv[var] %d\n", vps[var], bpv[var]);
    unsigned long long i, j, k, val_per_sample = 0;

    data[var] = malloc(sizeof (*(data[var])) * local_box_size[X] * local_box_size[Y] * local_box_size[Z] * (bpv[var]/8) * vps[var]);

    float fvalue = 0;
    double dvalue = 0;
    for (k = 0; k < local_box_size[Z]; k++)
      for (j = 0; j < local_box_size[Y]; j++)
        for (i = 0; i < local_box_size[X]; i++)
        {
          unsigned long long index = (unsigned long long) (local_box_size[X] * local_box_size[Y] * k) + (local_box_size[X] * j) + i;

          for (val_per_sample = 0; val_per_sample < vps[var]; val_per_sample++)
          {
            if ((bpv[var]) == 32)
            {
              fvalue = ((float)(100 + var + ((global_box_size[X] * global_box_size[Y]*(local_box_offset[Z] + k))+(global_box_size[X]*(local_box_offset[Y] + j)) + (local_box_offset[X] + i))));// / (512.0 * 512.0 * 512.0)) * 255.0;
              memcpy(data[var] + (index * vps[var] + val_per_sample) * sizeof(float), &fvalue, sizeof(float));
            }

            else if ((bpv[var]) == 64)
            {
              dvalue = ((double)100 + var + val_per_sample + ((global_box_size[X] * global_box_size[Y]*(local_box_offset[Z] + k))+(global_box_size[X]*(local_box_offset[Y] + j)) + (local_box_offset[X] + i)));
              memcpy(data[var] + (index * vps[var] + val_per_sample) * sizeof(double), &dvalue, sizeof(double));
            }
          }
        }
  }
}

//----------------------------------------------------------------
static void terminate()
{
#if PIDX_HAVE_MPI
  MPI_Abort(MPI_COMM_WORLD, -1);
#else
  exit(-1);
#endif
}

//----------------------------------------------------------------
static void terminate_with_error_msg(const char *format, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr, format);
  vfprintf(stderr, format, arg_ptr);
  va_end(arg_ptr);
  terminate();
}

//----------------------------------------------------------------
static void create_pidx_var_point_and_access()
{
  // Allocate a PIDX_variable array where we store the information 
  // of all the variables
  variable = (PIDX_variable*)malloc(sizeof(*variable) * variable_count);
  memset(variable, 0, sizeof(*variable) * variable_count);

  // Set variables that define the global and local domain information
  PIDX_set_point(global_size, global_box_size[X], global_box_size[Y], global_box_size[Z]);
  PIDX_set_point(local_offset, local_box_offset[X], local_box_offset[Y], local_box_offset[Z]);
  PIDX_set_point(local_size, local_box_size[X], local_box_size[Y], local_box_size[Z]);

  // Set variable that defines the restructuring box size
  PIDX_set_point(reg_size, rst_box_size[X], rst_box_size[Y], rst_box_size[Z]);

  //  Creating access
  PIDX_create_access(&p_access);
  PIDX_set_mpi_access(p_access, MPI_COMM_WORLD);

  return;
}

//----------------------------------------------------------------
static void set_pidx_file(int ts)
{
  PIDX_return_code ret;

  // Create IDX file 
  ret = PIDX_file_create(output_file_name, PIDX_MODE_CREATE, p_access, global_size, &file);
  if (ret != PIDX_success)  terminate_with_error_msg("PIDX_file_create\n");

  // Set the current timestep
  PIDX_set_current_time_step(file, ts);
  // Set the number of variables
  PIDX_set_variable_count(file, variable_count);

  // Advanced settings

  // Set the restructuring box size
  PIDX_set_restructuring_box(file, reg_size);
  
  // Select I/O mode (PIDX_IDX_IO for the multires, PIDX_RAW_IO for non-multires)
  PIDX_set_io_mode(file, PIDX_IDX_IO);
  
  // Set how many blocks we want to write in a single file
  PIDX_set_block_count(file, 256);
  
  // Set the size of a block: how many 2^N samples we want to put in a single block
  PIDX_set_block_size(file, 13);

  // If the domain decomposition and the cores configuration do not change over time 
  // we can instruct PIDX to cache and reuse these information for the next timesteps
  PIDX_set_cache_time_step(file, 0);

  //PIDX_set_compression_type(file, PIDX_CHUNKING_ZFP);
  //PIDX_set_lossy_compression_bit_rate(file, 32);

  return;
}

//----------------------------------------------------------------
static void set_pidx_variable(int var)
{
  PIDX_return_code ret = 0;

  // Set variable name, number of bits, typename
  ret = PIDX_variable_create(var_name[var], bpv[var] * vps[var], type_name[var], &variable[var]);
  if (ret != PIDX_success)  terminate_with_error_msg("PIDX_variable_create");

  // Set the variable offset and size of the local domain,
  // where the data is in memory (data) and what is its layout in memory (row major)
  ret = PIDX_variable_write_data_layout(variable[var], local_offset, local_size, data[var], PIDX_row_major);
  if (ret != PIDX_success)  terminate_with_error_msg("PIDX_variable_write_data_layout");

  // Tell PIDX that we want to write this variable
  ret = PIDX_append_and_write_variable(file, variable[var]);
  if (ret != PIDX_success)  terminate_with_error_msg("PIDX_append_and_write_variable");

  return;
}

//----------------------------------------------------------------
static void destroy_pidx_var_point_and_access()
{
  if (PIDX_close_access(p_access) != PIDX_success)
    terminate_with_error_msg("PIDX_close_access");

  free(variable);
  variable = 0;

  return;
}

//----------------------------------------------------------------
static void destroy_synthetic_simulation_data()
{
  int var = 0;
  for(var = 0; var < variable_count; var++)
  {
    free(data[var]);
    data[var] = 0;
  }
  free(data);
  data = 0;
}

//----------------------------------------------------------------
static void shutdown_mpi()
{
#if PIDX_HAVE_MPI
  MPI_Finalize();
#endif
}

