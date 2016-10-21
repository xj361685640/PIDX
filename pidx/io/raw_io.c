#include "../PIDX_inc.h"
#include "restructure.h"
#include "partition.h"
#include "local_buffer.h"
#include "headers.h"
#include "blocks.h"
#include "hz_buffer.h"
#include "agg_io.h"

static PIDX_return_code init(PIDX_io file, int gi);


PIDX_return_code PIDX_raw_write(PIDX_io file, int gi, int svi, int evi)
{
  PIDX_time time = file->idx_d->time;
  PIDX_return_code ret;

  // Calculate bounds with compression and
  // populate rank buffer
  time->idx_init_start = MPI_Wtime();
  ret = init(file, gi);
  if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}
  time->idx_init_end = MPI_Wtime();

  // Creates the file heirarchy
  // Also writes the header info for all binary files
  time->header_write_start = PIDX_get_time();
  ret = write_headers(file, gi, svi, evi, PIDX_RAW_IO);
  if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}
  time->header_write_end = PIDX_get_time();

  // Restructuring the grid into power two blocks
  // After this step every process has got a power two block
  // 15 x 31 x 10 ---> 16 x 32 x 16
  time->idx_rst_start = MPI_Wtime();
  ret = restructure_init(file, gi, svi, evi);
  if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}

  ret = restructure(file, PIDX_WRITE);
  if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}

  ret = restructure_io(file, PIDX_WRITE);
  if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}
  time->idx_rst_end = MPI_Wtime();

  // Cleanup all buffers nd ids
  time->buffer_cleanup_start = PIDX_get_time();
  ret = restructure_cleanup(file, gi);
  if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}
  time->buffer_cleanup_end = PIDX_get_time();

  time->EX = PIDX_get_time();

  return PIDX_success;
}


PIDX_return_code PIDX_raw_read(PIDX_io file, int gi, int svi, int evi)
{
  PIDX_time time = file->idx_d->time;
  PIDX_return_code ret;
  int nprocs = 1;

  // Calculate bounds with compression and
  // populate rank buffer
  time->idx_init_start = MPI_Wtime();
  ret = init(file, gi);
  if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}
  time->idx_init_end = MPI_Wtime();


  // Restructuring the grid into power two blocks
  // After this step every process has got a power two block
  // 15 x 31 x 10 ---> 16 x 32 x 16
  time->idx_rst_start = MPI_Wtime();

  MPI_Comm_size(file->comm, &nprocs);
  if (file->idx_d->data_core_count == nprocs)
  {
    ret = restructure_init(file, gi, svi, evi);
    if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}

    ret = restructure_io(file, PIDX_READ);
    if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}

    ret = restructure(file, PIDX_READ);
    if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}
    time->idx_rst_end = MPI_Wtime();

    // Cleanup all buffers nd ids
    time->buffer_cleanup_start = PIDX_get_time();
    ret = restructure_cleanup(file, gi);
    if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}
    time->buffer_cleanup_end = PIDX_get_time();
  }
  else
  {
    ret = restructure_forced_read(file, svi, evi);
    if (ret != PIDX_success) {fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__); return PIDX_err_file;}
  }

  time->EX = PIDX_get_time();

  return PIDX_success;
}


static PIDX_return_code init(PIDX_io file, int gi)
{
  int d = 0;
  int grank = 0, gnprocs = 1;
  PIDX_variable_group var_grp = file->idx->variable_grp[gi];

  for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
  {
    if (file->idx->bounds[d] % file->idx->chunk_size[d] == 0)
      file->idx->chunked_bounds[d] = (int) file->idx->bounds[d] / file->idx->chunk_size[d];
    else
      file->idx->chunked_bounds[d] = (int) (file->idx->bounds[d] / file->idx->chunk_size[d]) + 1;
  }

  MPI_Comm_rank(file->global_comm, &grank);
  MPI_Comm_size(file->global_comm, &gnprocs);

  var_grp->rank_buffer = malloc(gnprocs * sizeof(*var_grp->rank_buffer));
  memset(var_grp->rank_buffer, 0, gnprocs * sizeof(*var_grp->rank_buffer));
  MPI_Allgather(&grank, 1, MPI_INT, var_grp->rank_buffer, 1, MPI_INT, file->global_comm);

  return PIDX_success;
}
