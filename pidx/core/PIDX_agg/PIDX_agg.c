#include "../../PIDX_inc.h"
#include <zfp.h>

#define PIDX_MIN(a,b) (((a)<(b))?(a):(b))

#define PIDX_ACTIVE_TARGET 1

static PIDX_return_code report_error(PIDX_return_code ret, char* file, int line);
static PIDX_return_code create_window(PIDX_agg_id id, Agg_buffer ab);
static PIDX_return_code one_sided_data_com(PIDX_agg_id id, Agg_buffer ab, int layout_id, PIDX_block_layout lbl, int mode);
static PIDX_return_code aggregate(PIDX_agg_id id, int variable_index, unsigned long long hz_start_index, unsigned long long hz_count, unsigned char* hz_buffer, int buffer_offset, Agg_buffer ab, PIDX_block_layout lbl, int MODE, int layout_id);
static PIDX_return_code compressed_aggregate(PIDX_agg_id id, int variable_index, unsigned long long hz_start, unsigned long long hz_count, unsigned char* hz_buffer, int buffer_offset, Agg_buffer ab, PIDX_block_layout lbl, int MODE);
static int intersectNDChunk(Ndim_patch A, Ndim_patch B);
static PIDX_return_code decompress_aggregation_buffer(PIDX_agg_id id, Agg_buffer ab);
static PIDX_return_code block_wise_compression(PIDX_agg_id id, Agg_buffer ab, PIDX_block_layout lbl);

struct PIDX_agg_struct
{
  MPI_Win win;

  idx_comm idx_c;

  /// Contains all relevant IDX file info
  /// Blocks per file, samples per block, bitmask, patch, file name template and more
  idx_dataset idx;

  /// Contains all derieved IDX file info
  /// number of files, files that are ging to be populated
  idx_dataset_derived_metadata idx_d;

  int gi;
  int fi;
  int li;

  int ***agg_r;
};



PIDX_agg_id PIDX_agg_init(idx_dataset idx_meta_data, idx_dataset_derived_metadata idx_d, idx_comm idx_c, int fi, int li)
{
  PIDX_agg_id id;

  id = malloc(sizeof (*id));
  memset(id, 0, sizeof (*id));

  id->idx = idx_meta_data;
  id->idx_d = idx_d;
  id->idx_c = idx_c;

  id->gi = 0;
  id->fi = fi;
  id->li = li;

  return id;
}


PIDX_return_code PIDX_agg_meta_data_create(PIDX_agg_id id, Agg_buffer ab, PIDX_block_layout lbl)
{
  int i = 0, j = 0, j1 = 0, d = 0;

  PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];

  ab->aggregator_interval = id->idx_c->lnprocs / ((id->li - id->fi + 1) * lbl->efc * ab->agg_f);
  assert(ab->aggregator_interval != 0);

  ab->buffer_size = 0;
  ab->sample_number = -1;
  ab->var_number = -1;
  ab->file_number = -1;

  id->agg_r = malloc(lbl->efc * sizeof (int**));
  memset(id->agg_r, 0, lbl->efc * sizeof (int**));
  for (i = 0; i < lbl->efc; i++)
  {
    id->agg_r[i] = malloc((id->li - id->fi + 1) * sizeof (int*));
    memset(id->agg_r[i], 0, (id->li - id->fi + 1) * sizeof (int*));
    for (j = id->fi; j <= id->li; j++)
    {
      j1 = j - id->fi;
      PIDX_variable var = var_grp->variable[j];
      id->agg_r[i][j1] = malloc(var->vps * sizeof (int) * ab->agg_f);
      memset(id->agg_r[i][j1], 0, var->vps * sizeof (int) * ab->agg_f);
      for (d = 0 ; d < var->vps * ab->agg_f; d++)
        id->agg_r[i][j1][d] = -1;
    }
  }

  return PIDX_success;
}



PIDX_return_code PIDX_agg_buf_create_global_uniform_dist(PIDX_agg_id id, Agg_buffer ab, PIDX_block_layout lbl, int agg_offset, int var_offset, int file_status)
{
#if 1
  int i = 0, j = 0, k = 0;
  PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];
  int *agg_list;

  agg_list = malloc(sizeof(*agg_list) * id->idx_d->max_file_count * id->idx->variable_count);
  memset(agg_list, 0, sizeof(*agg_list) * id->idx_d->max_file_count * id->idx->variable_count);

  int M = id->idx_d->max_file_count * id->idx->variable_count;
  int N = id->idx_c->lnprocs;
  int interval = (N / M);

  if (interval >= 1)
  {
  for (i = 0; i < M; i++)
    agg_list[i] = i * interval;
  }
  else
  {
    // M = 32
    // N = 8
    interval = 1;
    for (k = 0; k < M/N; k++)
    {
      for (i = 0; i < N ; i++)
      {
        agg_list[N * k + i] = i * interval;
      }
    }
    i = 0;
    for (k = (M/N) * N; k < M; k++)
      agg_list[k] = i++;

  }

  /*
  int interval = ((N + 1) / M) * 2;
  int constant = ((N + 1) / M);
  for (i = 0; i < M/2; i++)
  {
    id->idx->agg_list[i] = i * interval;
  }
  for (i = M/2; i < M; i++)
  {
    id->idx->agg_list[i] = (i - M/2) * interval + constant;
  }
  */
  /*
  int interval = (N / M);
  for (i = 0; i < M; i++)
  {
    id->idx->agg_list[i] = i * interval + 1;
  }
  */

#if 0
  if (id->idx_c->lrank == 0 && agg_offset == 0)
  {
    printf("\nAggs: ");
    for (i = 0; i < id->idx_d->max_file_count * id->idx->variable_count; i++)
      printf ("%d ", agg_list[i]);
    printf("\n");
  }
#endif

  for (k = 0; k < lbl->efc; k++)
  {
    for (i = id->fi; i <= id->li; i++)
    {
      for (j = 0; j < var_grp->variable[i]->vps * ab->agg_f; j++)
      {
        id->agg_r[k][i - id->fi][j] = agg_list[id->idx->agg_counter];
        //if (id->idx_c->lrank == 0)
        //  printf("%d ----> %d\n", id->idx->agg_counter, agg_list[id->idx->agg_counter]);
        id->idx->agg_counter++;
      }
    }
  }
  free(agg_list);

  int aggregator_interval = id->idx_c->lnprocs / ((id->fi - id->li + 1) * lbl->efc);
  //int aggregator_interval = id->idx_c->lnprocs / ((id->fi - id->li + 1) * id->idx_d->max_file_count);

  for (k = 0; k < lbl->efc; k++)
  {
    for (i = id->fi; i <= id->li; i++)
    {
      for (j = 0; j < var_grp->variable[i]->vps * ab->agg_f; j++)
      {
        if(id->idx_c->lrank == id->agg_r[k][i - id->fi][j])
        {
          ab->file_number = lbl->existing_file_index[k];
          ab->var_number = i;
          ab->sample_number = j;

          unsigned long long sample_count = lbl->bcpf[ab->file_number] * id->idx_d->samples_per_block / ab->agg_f;

          int chunk_size = id->idx->chunk_size[0] * id->idx->chunk_size[1] * id->idx->chunk_size[2];

          int bpdt = 0;
          bpdt = (chunk_size * var_grp->variable[ab->var_number]->bpv/8) / (id->idx->compression_factor);

          ab->buffer_size = sample_count * bpdt;

          if (i == 0)
            printf("[Lid %d] [C %d] [G %d %d] [L %d %d] [Interval %d - %d / (%d * %d)] [V %d] [LFi %d] [GFi %d] [Si %d] [F/S/N %d]  [Buffer %lld (%d x %d x %d)]\n",
                 agg_offset, id->idx_d->color,
                 id->idx_c->grank, id->idx_c->gnprocs, id->idx_c->lrank, id->idx_c->lnprocs,
                 aggregator_interval, id->idx_c->lnprocs, (id->fi - id->li + 1), lbl->efc,
                 i,
                 k, lbl->existing_file_index[k],
                 j,
                 file_status,
                 ab->buffer_size, lbl->bcpf[ab->file_number], id->idx_d->samples_per_block, bpdt);


#if 0
          if (id->idx->cached_ts == id->idx->current_time_step)
            printf("[G %d] [%d] [L %d] [Lid %d] [V %d] [LFi %d] [GFi %d] [Si %d] [F/S/N %d] -> [[CR %d]] [Buffer %lld (%d x %d x %d)]\n", id->idx_c->grank, id->idx->file->idx_d->agg_counter, id->idx_c->lrank, agg_offset, i, k, lbl->existing_file_index[k], j, file_status, id->agg_r[k][i - id->fi][j], ab->buffer_size, lbl->bcpf[ab->file_number], id->idx_d->samples_per_block, bpdt);//, first[0], first[1], first[2], rank_x, rank_y, rank_z);
#endif
          ab->buffer = malloc(ab->buffer_size);
          memset(ab->buffer, 0, ab->buffer_size);
          if (ab->buffer == NULL)
          {
            fprintf(stderr, " Error in malloc %lld: Line %d File %s\n", (long long) ab->buffer_size, __LINE__, __FILE__);
            return PIDX_err_agg;
          }
        }
      }
    }
  }
#endif
  return PIDX_success;
}



PIDX_return_code PIDX_agg_buf_create_local_uniform_dist(PIDX_agg_id id, Agg_buffer ab, PIDX_block_layout lbl, int agg_offset, int var_offset, int file_status)
{
#if 1
  int i = 0, j = 0, k = 0;
  PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];

  int rank_counter = 0;
  int aggregator_interval = id->idx_c->lnprocs / ((id->fi - id->li + 1) * lbl->efc);

  for (k = 0; k < lbl->efc; k++)
  {
    for (i = id->fi; i <= id->li; i++)
    {
      for (j = 0; j < var_grp->variable[i]->vps * ab->agg_f; j++)
      {
        id->agg_r[k][i - id->fi][j] = rank_counter;
        rank_counter = rank_counter + aggregator_interval;

        if(id->idx_c->lrank == id->agg_r[k][i - id->fi][j])
        {
          ab->file_number = lbl->existing_file_index[k];
          ab->var_number = i;
          ab->sample_number = j;

          unsigned long long sample_count = lbl->bcpf[ab->file_number] * id->idx_d->samples_per_block / ab->agg_f;

          int chunk_size = id->idx->chunk_size[0] * id->idx->chunk_size[1] * id->idx->chunk_size[2];

          int bpdt = 0;
          bpdt = (chunk_size * var_grp->variable[ab->var_number]->bpv/8) / (id->idx->compression_factor);

          ab->buffer_size = sample_count * bpdt;

          if (i == 0)
            printf("[Lid %d] [C %d] [G %d %d] [L %d %d] [Interval %d - %d / (%d * %d)] [V %d] [LFi %d] [GFi %d] [Si %d] [F/S/N %d]  [Buffer %lld (%d x %d x %d)]\n",
                 agg_offset, id->idx_d->color,
                 id->idx_c->grank, id->idx_c->gnprocs, id->idx_c->lrank, id->idx_c->lnprocs,
                 aggregator_interval, id->idx_c->lnprocs, (id->fi - id->li + 1), lbl->efc,
                 i,
                 k, lbl->existing_file_index[k],
                 j,
                 file_status,
                 ab->buffer_size, lbl->bcpf[ab->file_number], id->idx_d->samples_per_block, bpdt);


#if 0
          if (id->idx->cached_ts == id->idx->current_time_step)
            printf("[G %d] [%d] [L %d] [Lid %d] [V %d] [LFi %d] [GFi %d] [Si %d] [F/S/N %d] -> [[CR %d]] [Buffer %lld (%d x %d x %d)]\n", id->idx_c->grank, id->idx->file->idx_d->agg_counter, id->idx_c->lrank, agg_offset, i, k, lbl->existing_file_index[k], j, file_status, id->agg_r[k][i - id->fi][j], ab->buffer_size, lbl->bcpf[ab->file_number], id->idx_d->samples_per_block, bpdt);//, first[0], first[1], first[2], rank_x, rank_y, rank_z);
#endif
          ab->buffer = malloc(ab->buffer_size);
          memset(ab->buffer, 0, ab->buffer_size);
          if (ab->buffer == NULL)
          {
            fprintf(stderr, " Error in malloc %lld: Line %d File %s\n", (long long) ab->buffer_size, __LINE__, __FILE__);
            return PIDX_err_agg;
          }
        }
      }
    }
  }
#endif
  return PIDX_success;
}



PIDX_return_code PIDX_agg_buf_create_localized_aggregation(PIDX_agg_id id, Agg_buffer ab, PIDX_block_layout lbl, int agg_offset, int var_offset, int file_status)
{
  PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];
  PIDX_variable var0 = var_grp->variable[id->fi];
  int i = 0, j = 0, k = 0;

  int max_patch_count;
  int patch_count =var0->patch_group_count;
  MPI_Allreduce(&patch_count, &max_patch_count, 1, MPI_INT, MPI_MAX, id->idx_c->local_comm);

  int *local_patch_offset = malloc(sizeof(uint32_t) * (max_patch_count * PIDX_MAX_DIMENSIONS));
  memset(local_patch_offset, 0, sizeof(uint32_t) * (max_patch_count * PIDX_MAX_DIMENSIONS));

  int *local_patch_size = malloc(sizeof(uint32_t) * (max_patch_count * PIDX_MAX_DIMENSIONS + 1));
  memset(local_patch_size, 0, sizeof(uint32_t) * (max_patch_count * PIDX_MAX_DIMENSIONS));

  int d = 0;
  for (i = 0; i < patch_count; i++)
  {
    for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
    {
      local_patch_offset[i * PIDX_MAX_DIMENSIONS + d] = (uint32_t)var0->rst_patch_group[i]->reg_patch->offset[d];
      local_patch_size[i * PIDX_MAX_DIMENSIONS + d] = (uint32_t)var0->rst_patch_group[i]->reg_patch->size[d];
    }
  }

  int wc = id->idx_c->lnprocs * (max_patch_count * PIDX_MAX_DIMENSIONS);

  int* global_patch_offset = malloc(wc * sizeof(uint32_t));
  memset(global_patch_offset, 0,wc * sizeof(uint32_t));

  int* global_patch_size = malloc(wc * sizeof(uint32_t));
  memset(global_patch_size, 0, wc * sizeof(uint32_t));

  MPI_Allgather(local_patch_offset, PIDX_MAX_DIMENSIONS * max_patch_count, MPI_INT, global_patch_offset, PIDX_MAX_DIMENSIONS * max_patch_count, MPI_INT, id->idx_c->local_comm);

  MPI_Allgather(local_patch_size, PIDX_MAX_DIMENSIONS * max_patch_count, MPI_INT, global_patch_size, PIDX_MAX_DIMENSIONS * max_patch_count, MPI_INT, id->idx_c->local_comm);

  free(local_patch_offset);
  free(local_patch_size);

  for (k = 0; k < lbl->efc; k++)
  {
    for (i = id->fi; i <= id->li; i++)
    {
      for (j = 0; j < var_grp->variable[i]->vps * ab->agg_f; j++)
      {
        int start_rank = -1, end_rank = -1;
        unsigned long long global_file_index = lbl->existing_file_index[k];

        int first_block = 0, last_block = 0;
        int b = 0;
        for (b = 0; b < id->idx->blocks_per_file; b++)
        {
          if (id->idx_d->block_bitmap[global_file_index][b] == 1)
          {
            first_block = b;
            break;
          }
        }
        for (b = id->idx->blocks_per_file - 1; b >= 0; b--)
        {
          if (id->idx_d->block_bitmap[global_file_index][b] == 1)
          {
            last_block = b;
            break;
          }
        }
        assert(last_block == lbl->lbi[global_file_index]);

        //printf("[%d] first last block %d %d [%d %d %d - %d %d %d]\n", id->idx_d->color, first_block, last_block, id->idx_d->partition_offset[0], id->idx_d->partition_offset[1], id->idx_d->partition_offset[2], (id->idx_d->partition_offset[0] + id->idx_d->partition_size[0] - 1), (id->idx_d->partition_offset[1] + id->idx_d->partition_size[1] - 1), (id->idx_d->partition_offset[2] + id->idx_d->partition_size[2] - 1));

        int s = 0;
        int last_index = 0;
        int first_index = 0;
        unsigned long long ZYX[PIDX_MAX_DIMENSIONS];

        if (id->idx_d->io_mode == 0)
        {
          for (s = 0; s < id->idx_d->samples_per_block; s++)
          {
            // HZ index of last sample of the block.
            last_index = global_file_index * id->idx->blocks_per_file * id->idx_d->samples_per_block + (lbl->lbi[global_file_index] + 1) * id->idx_d->samples_per_block - 1 - s;

            // xyz index of the last sample of the block.
            Hz_to_xyz(id->idx->bitPattern, id->idx_d->maxh - 1, last_index, ZYX);

            // check to see if the sample is within bounds.
            if (ZYX[0] < PIDX_MIN(id->idx->box_bounds[0], id->idx_d->partition_offset[0] + id->idx_d->partition_size[0]) &&
                ZYX[1] < PIDX_MIN(id->idx->box_bounds[1], id->idx_d->partition_offset[1] + id->idx_d->partition_size[1]) &&
                ZYX[2] < PIDX_MIN(id->idx->box_bounds[2], id->idx_d->partition_offset[2] + id->idx_d->partition_size[2]))
              break;
          }

          for (s = 0; s < id->idx_d->samples_per_block; s++)
          {
             // HZ index of first sample of the block.
            first_index = global_file_index * id->idx->blocks_per_file * id->idx_d->samples_per_block + (first_block) * id->idx_d->samples_per_block + s;

            // xyz index of the first sample of the block.
            Hz_to_xyz(id->idx->bitPattern, id->idx_d->maxh - 1, first_index, ZYX);

            // check to see if the sample is within bounds.
            if (ZYX[0] >= id->idx_d->partition_offset[0] && ZYX[0] < PIDX_MIN(id->idx->box_bounds[0], id->idx_d->partition_offset[0] + id->idx_d->partition_size[0]) &&
                ZYX[1] >= id->idx_d->partition_offset[1] && ZYX[1] < PIDX_MIN(id->idx->box_bounds[1], id->idx_d->partition_offset[1] + id->idx_d->partition_size[1]) &&
                ZYX[2] >= id->idx_d->partition_offset[2] && ZYX[2] < PIDX_MIN(id->idx->box_bounds[2], id->idx_d->partition_offset[2] + id->idx_d->partition_size[2]))
              break;
          }
        }
        else
        {
          for (s = 0; s < id->idx_d->samples_per_block; s++)
          {
            // HZ index of last sample of the block.
            last_index = global_file_index * id->idx->blocks_per_file * id->idx_d->samples_per_block + (lbl->lbi[global_file_index] + 1) * id->idx_d->samples_per_block - 1 - s;

            // xyz index of the last sample of the block.
            Hz_to_xyz(id->idx->bitPattern, id->idx_d->maxh - 1, last_index, ZYX);

            // check to see if the sample is within bounds.
            if (ZYX[0] < id->idx->box_bounds[0] &&
                ZYX[1] < id->idx->box_bounds[1] &&
                ZYX[2] < id->idx->box_bounds[2])
              break;
          }

          for (s = 0; s < id->idx_d->samples_per_block; s++)
          {
            // HZ index of first sample of the block.
            first_index = global_file_index * id->idx->blocks_per_file * id->idx_d->samples_per_block + (first_block) * id->idx_d->samples_per_block + s;

            // xyz index of the first sample of the block.
            Hz_to_xyz(id->idx->bitPattern, id->idx_d->maxh - 1, first_index, ZYX);

            // check to see if the sample is within bounds.
            if (ZYX[0] >= 0 && ZYX[0] < id->idx->box_bounds[0] &&
                ZYX[1] >= 0 && ZYX[1] < id->idx->box_bounds[1] &&
                ZYX[2] >= 0 && ZYX[2] < id->idx->box_bounds[2])
              break;
          }
        }

        unsigned long long global_start_hz = first_index;
        unsigned long long global_end_hz = last_index;

        unsigned long long global_start_ZYX[PIDX_MAX_DIMENSIONS], global_end_ZYX[PIDX_MAX_DIMENSIONS];

        Hz_to_xyz(id->idx->bitPattern, id->idx_d->maxh - 1, global_start_hz, global_start_ZYX);
        Hz_to_xyz(id->idx->bitPattern, id->idx_d->maxh - 1, global_end_hz, global_end_ZYX);

        Ndim_patch global_start_point = (Ndim_patch)malloc(sizeof (*global_start_point));
        memset(global_start_point, 0, sizeof (*global_start_point));
        global_start_point->offset[0] = global_start_ZYX[0];
        global_start_point->offset[1] = global_start_ZYX[1];
        global_start_point->offset[2] = global_start_ZYX[2];
        global_start_point->size[0] = 1;
        global_start_point->size[1] = 1;
        global_start_point->size[2] = 1;

        //Extent of process with rank r
        Ndim_patch rank_r_patch = malloc(sizeof (*rank_r_patch));
        memset(rank_r_patch, 0, sizeof (*rank_r_patch));

        int r = 0, d = 0, m = 0;
        int break_counter = 0;
        for (r = 0; r < id->idx_c->lnprocs; r++)
        {
          for (m = 0; m < max_patch_count; m++)
          {
            if (global_patch_size[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + 0] == 0 &&
                global_patch_size[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + 1] == 0 &&
                global_patch_size[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + 2] == 0)
              continue;

            for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
            {
              rank_r_patch->offset[d] = global_patch_offset[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + d];
              rank_r_patch->size[d] = global_patch_size[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + d];
            }

            if (intersectNDChunk(global_start_point, rank_r_patch))
            {
              start_rank = r;
              break_counter = 1;
              break;
            }
          }

          if (break_counter == 1)
            break;
        }
        free(global_start_point);

        Ndim_patch global_end_point = (Ndim_patch)malloc(sizeof (*global_end_point));
        memset(global_end_point, 0, sizeof (*global_end_point));
        global_end_point->offset[0] = global_end_ZYX[0];
        global_end_point->offset[1] = global_end_ZYX[1];
        global_end_point->offset[2] = global_end_ZYX[2];
        global_end_point->size[0] = 1;
        global_end_point->size[1] = 1;
        global_end_point->size[2] = 1;

        break_counter = 0;
        memset(rank_r_patch, 0, sizeof (*rank_r_patch));
        for (r = 0; r < id->idx_c->lnprocs; r++)
        {
          for (m = 0; m < max_patch_count; m++)
          {
            if (global_patch_size[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + 0] == 0 &&
                global_patch_size[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + 1] == 0 &&
                global_patch_size[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + 2] == 0)
              continue;

            for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
            {
              rank_r_patch->offset[d] = global_patch_offset[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + d];
              rank_r_patch->size[d] = global_patch_size[PIDX_MAX_DIMENSIONS * max_patch_count * r + m * PIDX_MAX_DIMENSIONS + d];
            }

            if (intersectNDChunk(global_end_point, rank_r_patch))
            {
              end_rank = r;
              break_counter = 1;
              break;
            }
          }
          if (break_counter == 1)
            break;
        }
        free(rank_r_patch);
        free(global_end_point);

        int range = (end_rank - start_rank + 1) / id->idx->variable_count;
        if (file_status == 1)
          id->agg_r[k][i - id->fi][j] = start_rank + (i * range) + (range/2);
        else if (file_status == 0)
          id->agg_r[k][i - id->fi][j] = start_rank + (i * range);
        else if (file_status == 2)
          id->agg_r[k][i - id->fi][j] = id->idx_c->gnprocs - 1;
#if 0
        printf("%d %d -> %d\n", id->idx_c->lrank, id->idx_c->grank, id->agg_r[k][i - id->fi][j]);
        printf("XX: [Lid %d] [C %d] [G %d %d] [L %d %d] [S E R %d (%d : %d %d %d) - %d (%d (%d %d) : %d %d %d) %d] [V %d] [LFi %d] [GFi %d] [Si %d] [F/S/N %d]\n",
             agg_offset, id->idx_d->color,
             id->idx_c->grank, id->idx_c->gnprocs, id->idx_c->lrank, id->idx_c->lnprocs,
             start_rank, global_start_hz, global_start_ZYX[0], global_start_ZYX[1], global_start_ZYX[2],
             end_rank, global_end_hz, global_file_index, lbl->lbi[global_file_index], global_end_ZYX[0], global_end_ZYX[1], global_end_ZYX[2],
             range,
             i,
             k, lbl->existing_file_index[k],
             j,
             file_status);
#endif
        if (id->idx_c->lrank == id->agg_r[k][i - id->fi][j])
        {
          ab->file_number = lbl->existing_file_index[k];
          ab->var_number = i;
          ab->sample_number = j;

          unsigned long long sample_count = lbl->bcpf[ab->file_number] * id->idx_d->samples_per_block / ab->agg_f;
          int chunk_size = id->idx->chunk_size[0] * id->idx->chunk_size[1] * id->idx->chunk_size[2];
          int bpdt = (chunk_size * var_grp->variable[ab->var_number]->bpv/8) / (id->idx->compression_factor);

          ab->buffer_size = sample_count * bpdt;

          if (i == 0)
            printf("[Lid %d] [C %d] [G %d %d] [L %d %d] [S E R %d - %d : %d] [BL %d %d] [HZ %lld %lld] [%lld %lld %lld : %lld %lld %lld] [V %d] [LFi %d] [GFi %d] [Si %d] [F/S/N %d]  [Buffer %lld (%d x %d x %d)]\n",
                 agg_offset, id->idx_d->color,
                 id->idx_c->grank, id->idx_c->gnprocs, id->idx_c->lrank, id->idx_c->lnprocs,
                 start_rank, end_rank, range,
                 first_block, last_block,
                 global_start_hz, global_end_hz,
                 global_start_ZYX[0], global_start_ZYX[1], global_start_ZYX[2],
                 global_end_ZYX[0], global_end_ZYX[1], global_end_ZYX[2],
                 i,
                 k, lbl->existing_file_index[k],
                 j,
                 file_status,
                 ab->buffer_size, lbl->bcpf[ab->file_number], id->idx_d->samples_per_block, bpdt);

          ab->buffer = malloc(ab->buffer_size);
          memset(ab->buffer, 0, ab->buffer_size);
          if (ab->buffer == NULL)
          {
            fprintf(stderr, " Error in malloc %lld: Line %d File %s\n", (long long) ab->buffer_size, __LINE__, __FILE__);
            return PIDX_err_agg;
          }
        }
      }
    }
  }

  free(global_patch_size);
  free(global_patch_offset);

  return PIDX_success;
}


PIDX_return_code PIDX_agg_global_and_local(PIDX_agg_id id, Agg_buffer ab, int layout_id, PIDX_block_layout lbl,  int MODE)
{
  int ret;
  ret = create_window(id, ab);
  if (ret != PIDX_success) report_error(PIDX_err_agg, __FILE__, __LINE__);

#ifdef PIDX_ACTIVE_TARGET
  ret = MPI_Win_fence(0, id->win);
  if (ret != MPI_SUCCESS) report_error(PIDX_err_agg, __FILE__, __LINE__);
#endif

  ret = one_sided_data_com(id, ab, layout_id, lbl, MODE);
  if (ret != PIDX_success) report_error(PIDX_err_agg, __FILE__, __LINE__);

#if PIDX_HAVE_MPI
#ifdef PIDX_ACTIVE_TARGET
  ret = MPI_Win_fence(0, id->win);
  if (ret != MPI_SUCCESS) report_error(PIDX_err_agg, __FILE__, __LINE__);
#endif

  ret = MPI_Win_free(&(id->win));
  if (ret != MPI_SUCCESS) report_error(PIDX_err_agg, __FILE__, __LINE__);
#endif

  if (id->idx->compression_type == PIDX_ZFP_COMPRESSION)
  {
    ret = decompress_aggregation_buffer(id, ab);
    if (ret != MPI_SUCCESS) report_error(PIDX_err_agg, __FILE__, __LINE__);

    //ret = block_wise_compression(id, ab, lbl);
    //if (ret != MPI_SUCCESS) report_error(PIDX_err_agg, __FILE__, __LINE__);
  }

  return PIDX_success;
}


PIDX_return_code PIDX_agg_meta_data_destroy(PIDX_agg_id id, PIDX_block_layout lbl)
{
  int i = 0, j = 0;
  for (i = 0; i < lbl->efc; i++)
  {
    for (j = id->fi; j <= id->li; j++)
    {
      free(id->agg_r[i][j - id->fi]);
      id->agg_r[i][j - id->fi] = 0;
    }
    free(id->agg_r[i]);
  }
  free(id->agg_r);
  id->agg_r = 0;

  return PIDX_success;
}


PIDX_return_code PIDX_agg_buf_destroy(Agg_buffer ab)
{
  if (ab->buffer_size != 0)
  {
    free(ab->buffer);
    ab->buffer = 0;
  }

  return PIDX_success;
}


PIDX_return_code PIDX_agg_finalize(PIDX_agg_id id)
{
  free(id);
  id = 0;

  return PIDX_success;
}


static PIDX_return_code create_window(PIDX_agg_id id, Agg_buffer ab)
{
  int ret = 0;

  PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];
  PIDX_variable var = var_grp->variable[ab->var_number];

  if (ab->buffer_size != 0)
  {
    int tcs = id->idx->chunk_size[0] * id->idx->chunk_size[1] * id->idx->chunk_size[2];
    int bpdt = tcs * (var->bpv/8) / (id->idx->compression_factor);

    ret = MPI_Win_create(ab->buffer, ab->buffer_size, bpdt, MPI_INFO_NULL, id->idx_c->local_comm, &(id->win));
    if (ret != MPI_SUCCESS) report_error(PIDX_err_agg, __FILE__, __LINE__);
  }
  else
  {
    ret = MPI_Win_create(0, 0, 1, MPI_INFO_NULL, id->idx_c->local_comm, &(id->win));
    if (ret != MPI_SUCCESS) report_error(PIDX_err_agg, __FILE__, __LINE__);
  }

  return PIDX_success;
}


static PIDX_return_code decompress_aggregation_buffer(PIDX_agg_id id, Agg_buffer ab)
{
  if (ab->buffer_size != 0)
  {
    PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];
    size_t dtype_bytes = var_grp->variable[ab->var_number]->bpv / 8;
    zfp_type type = (dtype_bytes == 4) ? zfp_type_float : zfp_type_double;
    unsigned char* buf = ab->buffer;
    size_t offset = id->idx_d->samples_per_block * dtype_bytes;
    while (offset < ab->buffer_size)
    {
      unsigned int compressed_bytes = *(unsigned int*)(buf + offset);
      if (compressed_bytes != 0)
      {
        int dim_x = ((int*)(buf + offset))[1];
        int dim_y = ((int*)(buf + offset))[2];
        int dim_z = ((int*)(buf + offset))[3];
        unsigned char* input = (unsigned char*)malloc(compressed_bytes);
        memcpy(input, buf + offset + 16, compressed_bytes);
        zfp_field* field = zfp_field_3d(buf + offset, type, dim_x, dim_y, dim_z);
        zfp_stream* zfp = zfp_stream_open(NULL);
        bitstream* stream = stream_open(input, compressed_bytes);
        zfp_stream_set_accuracy(zfp, 0, type);
        zfp_stream_set_bit_stream(zfp, stream);
        if (!zfp_decompress(zfp, field))
          puts("ERROR: Something wrong happened during decompression\n");
        zfp_field_free(field);
        zfp_stream_close(zfp);
        stream_close(stream);
        free(input);
        size_t original_bytes = dtype_bytes * dim_x * dim_y * dim_z;
        offset += original_bytes;
      }
      else
      {
        offset += sizeof(int);
      }
    }
  }
  return PIDX_success;
}

#undef max
#define max(a,b) ((a) > (b) ? (a) : (b))
static Point3D get_strides(const char* bit_string, int bs_len, int len)
{
  assert(len >= 0);
  Point3D stride = { 0, 0, 0 };
  size_t start = max(bs_len - len, 0);
  for (size_t i = start; i < bs_len; ++i)
  {
    if      (bit_string[i] == '0') { ++stride.x; }
    else if (bit_string[i] == '1') { ++stride.y; }
    else if (bit_string[i] == '2') { ++stride.z; }
  }
  if (len > bs_len) { ++stride.x; ++stride.y; ++stride.z; }
  stride.x = 1 << stride.x;
  stride.y = 1 << stride.y;
  stride.z = 1 << stride.z;
  return stride;
}
#undef max

static Point3D get_intra_block_strides(const char* bit_string, int bs_len, int hz_level)
{
  // count the number of x, y, z in the least significant (z_level + 1) bits
  // in the bit_string
  int z_level = bs_len - hz_level;
  int len = z_level + 1;
  return get_strides(bit_string, bs_len, len);
}

/* Return the strides (in terms of the first sample) of idx blocks, in x, y, and z. */
static Point3D get_inter_block_strides(const char* bit_string, int bs_len, int hz_level, int bits_per_block)
{
  assert(bs_len >= hz_level);
  // count the number of x, y, z in the least significant
  // (z_level + bits_per_block + 1) bits in the bit_string
  int len = bs_len - hz_level + bits_per_block + 1;
  // len can get bigger than bit_string.size if the input hz_level is smaller
  // than the mininum hz level
  return get_strides(bit_string, bs_len, len);
}

/* Get the number of samples in each dimension for a block at the given hz level */
static Point3D get_num_samples_per_block(const char* bit_string, int bs_len, int hz_level, int bits_per_block)
{
  Point3D intra_stride = get_intra_block_strides(bit_string, bs_len, hz_level);
  Point3D inter_stride = get_inter_block_strides(bit_string, bs_len, hz_level, bits_per_block);
  Point3D block_nsamples;
  block_nsamples.x = inter_stride.x / intra_stride.x;
  block_nsamples.y = inter_stride.y / intra_stride.y;
  block_nsamples.z = inter_stride.z / intra_stride.z;
  return block_nsamples;
}

static PIDX_return_code block_wise_compression(PIDX_agg_id id, Agg_buffer ab, PIDX_block_layout lbl)
{
  int hz_level = lbl->resolution_from;
  // TODO: here we assume the bit string contains a 'V' in the beginning
  const char* bit_string = id->idx->bitSequence + 1;
  int bs_len = strlen(bit_string) - 1;
  int bits_per_block = id->idx->bits_per_block;
  Point3D block_nsamples = get_num_samples_per_block(bit_string, bs_len, hz_level, bits_per_block);
  assert(block_nsamples.x * block_nsamples.y * block_nsamples.z == id->idx_d->samples_per_block);
  if (block_nsamples.x * block_nsamples.y * block_nsamples.z != id->idx_d->samples_per_block)
    puts("ERROR: Wrong number of samples per block\n");
  PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];
  if (ab->buffer_size != 0)
  {
    size_t dtype_bytes = var_grp->variable[ab->var_number]->bpv / 8;
    int i = 0; // starting block
    size_t offset = 0;
    if (ab->file_number == 0) {
      offset = dtype_bytes * id->idx_d->samples_per_block; // we skip the first block
      i = 1; // skip the first block
    }
    size_t original_bytes = id->idx_d->samples_per_block * dtype_bytes;
    unsigned char* temp = (unsigned char*)malloc(original_bytes);
    for (; i < lbl->bcpf[ab->file_number]; i++)
    {
      void* buf = ab->buffer + i * dtype_bytes * id->idx_d->samples_per_block;
      zfp_type type = (dtype_bytes == 4) ? zfp_type_float : zfp_type_double;
      zfp_field* field = zfp_field_3d(buf, type, block_nsamples.x, block_nsamples.y, block_nsamples.z);
      zfp_stream* zfp = zfp_stream_open(NULL);
      zfp_stream_set_accuracy(zfp, 0, type);
      size_t max_compressed_bytes = zfp_stream_maximum_size(zfp, field);
      if (max_compressed_bytes > original_bytes)
        puts("WARNING: compressed size potentially > original size\n");
      bitstream* stream = stream_open(temp, original_bytes);
      zfp_stream_set_bit_stream(zfp, stream);
      size_t compressed_bytes = zfp_compress(zfp, field);
      if (compressed_bytes == 0)
        puts("ERROR: Something wrong happened during compression\n");
      if (compressed_bytes > original_bytes)
        puts("WARNING: compressed size > original size\n");
      memcpy(ab->buffer + offset, temp, compressed_bytes);
      offset += compressed_bytes;
      zfp_field_free(field);
      zfp_stream_close(zfp);
      stream_close(stream);
    }
    free(temp);
    if (offset > ab->buffer_size)
        puts("WARNING: compressed buffer size > original buffer size\n");
    ab->buffer_size = ab->compressed_buffer_size = offset;
  }
  return PIDX_success;
}

static PIDX_return_code one_sided_data_com(PIDX_agg_id id, Agg_buffer ab, int layout_id, PIDX_block_layout lbl, int mode)
{
  int i, p, v, ret = 0;
  unsigned long long index = 0, count = 0;

  PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];
  PIDX_variable var0 = var_grp->variable[id->fi];

  for(v = id->fi; v <= id->li; v++)
  {
    PIDX_variable var = var_grp->variable[v];
    for (p = 0; p < var->patch_group_count; p++)
    {
      index = 0, count = 0;
      HZ_buffer hz_buf = var->hz_buffer[p];

      if (hz_buf->type == 1)
      {
#ifdef PIDX_DUMP_AGG
        if (id->idx_d->dump_agg_info == 1 && id->idx->current_time_step == 0)
        {
          fprintf(agg_dump_fp, "Type %d Variable %d Patch %d\n",hz_buf->type, v, p);
          fflush(agg_dump_fp);
        }
#endif

        for (i = lbl->resolution_from; i < lbl->resolution_to; i++)
        {
          if (hz_buf->nsamples_per_level[i][0] * hz_buf->nsamples_per_level[i][1] * hz_buf->nsamples_per_level[i][2] != 0)
          {
            index = 0;
            if (id->idx->compression_type == PIDX_ZFP_COMPRESSION)
              count = hz_buf->compressed_buffer_size[i];
            else
              count = hz_buf->end_hz_index[i] - hz_buf->start_hz_index[i] + 1;

            //printf("L [%d] : %d : %d [%d - %d]\n", i, hz_buf->compressed_buffer_size[i], (hz_buf->end_hz_index[i] - hz_buf->start_hz_index[i] + 1), hz_buf->end_hz_index[i], hz_buf->start_hz_index[i]);
#ifdef PIDX_DUMP_AGG
            if (id->idx_d->dump_agg_info == 1 && id->idx->current_time_step == 0)
            {
              fprintf(agg_dump_fp, "[%d]: ", i);
              fflush(agg_dump_fp);
            }
#endif
            //printf("A [Level %d] : Offset %d Count %d\n", i, hz_buf->start_hz_index[i], count);
            if (id->idx->compression_type == PIDX_ZFP_COMPRESSION)
              ret = compressed_aggregate(id, v, hz_buf->start_hz_index[i], count, hz_buf->buffer[i], 0, ab, lbl, mode);
            else
              ret = aggregate(id, v, hz_buf->start_hz_index[i], count, hz_buf->buffer[i], 0, ab, lbl, mode, layout_id);
            if (ret != PIDX_success)
            {
              fprintf(stderr, " Error in aggregate Line %d File %s\n", __LINE__, __FILE__);
              return PIDX_err_agg;
            }
          }
        }
      }
      else if (hz_buf->type == 2)
      {
#ifdef PIDX_DUMP_AGG
        if (id->idx_d->dump_agg_info == 1 && id->idx->current_time_step == 0)
        {
          fprintf(agg_dump_fp, "Type %d Variable %d Patch %d\n",hz_buf->type, v, p);
          fflush(agg_dump_fp);
        }
#endif

        for (i = lbl->resolution_from; i < lbl->resolution_to; i++)
        {
          if (var0->hz_buffer[p]->nsamples_per_level[i][0] * var0->hz_buffer[p]->nsamples_per_level[i][1] * var0->hz_buffer[p]->nsamples_per_level[i][2] != 0)
          {
#ifdef PIDX_DUMP_AGG
            if (id->idx_d->dump_agg_info == 1 && id->idx->current_time_step == 0)
            {
              fprintf(agg_dump_fp, "[%d]: ", i);
              fflush(agg_dump_fp);
            }
#endif
            int start_block_index = hz_buf->start_hz_index[i] / id->idx_d->samples_per_block;
            int end_block_index = hz_buf->end_hz_index[i] / id->idx_d->samples_per_block;
            assert(start_block_index >= 0 && end_block_index >= 0 && start_block_index <= end_block_index);

            if (end_block_index == start_block_index)
            {
              count = (hz_buf->end_hz_index[i] - hz_buf->start_hz_index[i] + 1);
              //printf("B [Level %d] : Offset %d Count %d\n", i, hz_buf->start_hz_index[i], count);
              ret = aggregate(id, v, var0->hz_buffer[p]->start_hz_index[i], count, hz_buf->buffer[i], 0, ab, lbl, mode, layout_id);
              if (ret != PIDX_success)
              {
                fprintf(stderr, " Error in aggregate Line %d File %s\n", __LINE__, __FILE__);
                return PIDX_err_agg;
              }
            }
            //
            else
            {
              int send_index = 0;
              int bl;
              for (bl = start_block_index; bl <= end_block_index; bl++)
              {
                if (PIDX_blocks_is_block_present(bl, lbl))
                {
                  if (bl == start_block_index)
                  {
                    index = 0;
                    count = ((start_block_index + 1) * id->idx_d->samples_per_block) - hz_buf->start_hz_index[i];
                  }
                  else if (bl == end_block_index)
                  {
                    index = (end_block_index * id->idx_d->samples_per_block - hz_buf->start_hz_index[i]);
                    count = hz_buf->end_hz_index[i] - ((end_block_index) * id->idx_d->samples_per_block) + 1;
                  }
                  else
                  {
                    index = (bl * id->idx_d->samples_per_block - hz_buf->start_hz_index[i]);
                    count = id->idx_d->samples_per_block;
                  }

                  //printf("C [Level %d] : Offset %d Count %d\n", i, hz_buf->start_hz_index[i], count);
                  ret = aggregate(id, v, index + hz_buf->start_hz_index[i], count, hz_buf->buffer[i], send_index, ab, lbl, mode, layout_id);
                  if (ret != PIDX_success)
                  {
                    fprintf(stderr, "[%s] [%d] write_read_samples() failed.\n", __FILE__, __LINE__);
                    return PIDX_err_agg;
                  }
                  send_index = send_index + count;
                }
                else
                  send_index = send_index + id->idx_d->samples_per_block;
              }
            }
            //
          }
        }
      }
    }
  }

  return PIDX_success;
}

static PIDX_return_code aggregate(PIDX_agg_id id, int variable_index, unsigned long long hz_start, unsigned long long hz_count, unsigned char* hz_buffer, int buffer_offset, Agg_buffer ab, PIDX_block_layout lbl, int MODE, int layout_id)
{
  int ret;
  int itr;
  int bpdt;
  int file_no = 0, block_no = 0, negative_block_offset = 0, sample_index = 0, vps;
  int target_rank = 0;
  unsigned long long start_agg_index = 0, end_agg_index = 0, target_disp = 0, target_count = 0, samples_in_file = 0;
  unsigned long long samples_per_file = (unsigned long long) id->idx_d->samples_per_block * id->idx->blocks_per_file;

  unsigned long long tcs = id->idx->chunk_size[0] * id->idx->chunk_size[1] * id->idx->chunk_size[2];


  PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];
  PIDX_variable var = var_grp->variable[variable_index];

  vps = var->vps; //number of samples for variable j

  // hz_start is the starting HZ index for the data buffer at level "level" and for regular patch number "patch"
  //file number to which the first element of the buffer belongs to
  file_no = hz_start / samples_per_file;

  //block number for the first element of the buffer
  block_no = hz_start / id->idx_d->samples_per_block;


  //number of empty blocks befor block "block_no" in the file "file_no"
  //negative_block_offset = PIDX_blocks_find_negative_offset(id->idx->blocks_per_file, block_no, id->idx->variable[id->ini]->global_block_layout);
  negative_block_offset = PIDX_blocks_find_negative_offset(id->idx->blocks_per_file, block_no, lbl);
  if (negative_block_offset < 0)
    return PIDX_err_agg;

  //number of samples in file "file_no"
  //samples_in_file = id->idx->variable[id->ini]->bcpf[file_no] * id->idx_d->samples_per_block;
  samples_in_file = lbl->bcpf[file_no] * id->idx_d->samples_per_block;
  if (samples_in_file > samples_per_file)
    return PIDX_err_agg;

  //Calculating the hz index of "hz_start" relative to the file to which it belongs also taking into account empty blocks in file
  target_disp = ((hz_start - ((samples_per_file * file_no) + (negative_block_offset * id->idx_d->samples_per_block))) * vps)
      %
      (samples_in_file * vps);

  sample_index = target_disp / (samples_in_file / ab->agg_f);
  if (sample_index >= var->vps * ab->agg_f)
    return PIDX_err_agg;

  target_disp = target_disp % (samples_in_file / ab->agg_f);

  //if (rank == 60)
  //  printf("%d ----> %d TD %d NBO %d\n", hz_start, block_no, target_disp, negative_block_offset);

  target_rank = id->agg_r[lbl->inverse_existing_file_index[file_no]][variable_index - id->fi][sample_index];
  //if (id->idx_c->grank == 0)
  //  printf("File no %d IFI %d TR %d: ", file_no, lbl->inverse_existing_file_index[file_no], target_rank);

  /*
  if (layout_id != 0 && id->idx->current_time_step == 0)
  {
  MPI_Comm agg_comm;
  int max_rank = 0;
  int min_rank = 0;

  MPI_Comm_split(id->local_comm, target_rank, rank, &agg_comm);
  int nrank = 0;
  MPI_Comm_rank(agg_comm, &nrank);

  MPI_Allreduce(&rank, &max_rank, 1, MPI_INT, MPI_MAX, agg_comm);
  MPI_Allreduce(&rank, &min_rank, 1, MPI_INT, MPI_MIN, agg_comm);

  MPI_Comm_free(&agg_comm);

  if (target_rank < min_rank || target_rank > max_rank || rank < min_rank || rank > max_rank)
  {
    printf("[TR %d] [%d] V %d P %d A %d FN %d [%d %d]\n", target_rank, rank, variable_index, layout_id, lbl->inverse_existing_file_index[file_no], file_no, min_rank, max_rank);
  }

  assert(target_rank >= min_rank);
  assert(target_rank <= max_rank);
  assert(rank >= min_rank);
  assert(rank <= max_rank);
  }
  */

  target_count = hz_count * vps;
  bpdt = ((var->bpv / 8) * tcs) / (id->idx->compression_factor);
  hz_buffer = hz_buffer + buffer_offset * bpdt * vps;

  start_agg_index = target_disp / (unsigned long long) (samples_in_file / ab->agg_f);
  end_agg_index = ((target_disp + target_count - 1) / (unsigned long long) (samples_in_file / ab->agg_f));

  //printf("SAI : EAI TR %d FN %d IFN %d :: %d : %d\n", target_rank, file_no, lbl->inverse_existing_file_index[file_no], start_agg_index, end_agg_index);
  if (start_agg_index != end_agg_index)
  {
    if (target_rank != id->idx_c->lrank)
    {
#ifndef PIDX_ACTIVE_TARGET
      MPI_Win_lock(MPI_LOCK_SHARED, target_rank, 0 , id->win);
#endif
      //target_disp_address = target_disp;
      if (MODE == PIDX_WRITE)
      {
        ret = MPI_Put(hz_buffer, ((samples_in_file / ab->agg_f) - target_disp) * bpdt, MPI_BYTE, target_rank, target_disp, ( (samples_in_file / ab->agg_f) - target_disp) * bpdt, MPI_BYTE, id->win);
        if(ret != MPI_SUCCESS)
        {
          fprintf(stderr, " Error in MPI_Put Line %d File %s\n", __LINE__, __FILE__);
          return PIDX_err_agg;
        }
      }
      else
      {
        printf("[1] TR %d\n", target_rank);
        ret = MPI_Get(hz_buffer, ((samples_in_file / ab->agg_f) - target_disp) * bpdt, MPI_BYTE, target_rank, target_disp, ( (samples_in_file / ab->agg_f) - target_disp) * bpdt, MPI_BYTE, id->win);
        if(ret != MPI_SUCCESS)
        {
          fprintf(stderr, " Error in MPI_Get Line %d File %s\n", __LINE__, __FILE__);
          return PIDX_err_agg;
        }
      }

#ifndef PIDX_ACTIVE_TARGET
      MPI_Win_unlock(target_rank, id->win);
#endif
    }
    else
    {
      if (MODE == PIDX_WRITE)
      {
        memcpy( ab->buffer + target_disp * bpdt, hz_buffer, ( (samples_in_file / ab->agg_f) - target_disp) * bpdt);
      }
      else
      {
        memcpy( hz_buffer, ab->buffer + target_disp * bpdt, ( (samples_in_file / ab->agg_f) - target_disp) * bpdt);
      }
    }

    for (itr = 0; itr < end_agg_index - start_agg_index - 1; itr++)
    {
      if (target_rank != id->idx_c->lrank)
      {
#ifndef PIDX_ACTIVE_TARGET
        MPI_Win_lock(MPI_LOCK_SHARED, target_rank + ab->aggregator_interval, 0, id->win);
#endif
        if (MODE == PIDX_WRITE)
        {
          ret = MPI_Put(hz_buffer + (( (samples_in_file / ab->agg_f) - target_disp) + (itr * (samples_in_file / ab->agg_f))) * bpdt, (samples_in_file / ab->agg_f) * bpdt, MPI_BYTE, target_rank + ab->aggregator_interval, 0, (samples_in_file / ab->agg_f) * bpdt, MPI_BYTE, id->win);
          if (ret != MPI_SUCCESS)
          {
            fprintf(stderr, " Error in MPI_Put Line %d File %s\n", __LINE__, __FILE__);
            return PIDX_err_agg;
          }
        }
        else
        {
          printf("[2] TR %d\n", target_rank + ab->aggregator_interval);
          ret = MPI_Get(hz_buffer + (((samples_in_file / ab->agg_f) - target_disp) + (itr * (samples_in_file / ab->agg_f))) * bpdt, (samples_in_file / ab->agg_f) * bpdt, MPI_BYTE, target_rank + ab->aggregator_interval, 0, (samples_in_file / ab->agg_f) * bpdt, MPI_BYTE, id->win);
          if (ret != MPI_SUCCESS)
          {
            fprintf(stderr, " Error in MPI_Get Line %d File %s\n", __LINE__, __FILE__);
            return PIDX_err_agg;
          }
        }
#ifndef PIDX_ACTIVE_TARGET
        MPI_Win_unlock(target_rank + ab->aggregator_interval, id->win);
#endif
      }
      else
      {
        if (MODE == PIDX_WRITE)
        {
          memcpy( ab->buffer, hz_buffer + (( (samples_in_file / ab->agg_f) - target_disp) + (itr * (samples_in_file / ab->agg_f))) * bpdt, (samples_in_file / ab->agg_f) * bpdt);
        }
        else
        {
          memcpy( hz_buffer + (((samples_in_file / ab->agg_f) - target_disp) + (itr * (samples_in_file / ab->agg_f))) * bpdt, ab->buffer, (samples_in_file / ab->agg_f) * bpdt);
        }
      }
    }

    if (target_rank + ab->aggregator_interval != id->idx_c->lrank)
    {
#ifndef PIDX_ACTIVE_TARGET
      MPI_Win_lock(MPI_LOCK_SHARED, target_rank + ab->aggregator_interval, 0, id->win);
#endif
      if (MODE == PIDX_WRITE)
      {
        ret = MPI_Put(hz_buffer + (((samples_in_file / ab->agg_f) - target_disp) + ((end_agg_index - start_agg_index - 1) * (samples_in_file / ab->agg_f))) * bpdt, (target_count - (((end_agg_index - start_agg_index - 1) * ((samples_in_file / ab->agg_f))) + (((samples_in_file / ab->agg_f)) - target_disp))) * bpdt, MPI_BYTE, target_rank + ab->aggregator_interval, 0, (target_count - ((end_agg_index - start_agg_index) * (samples_in_file / ab->agg_f) - target_disp)) * bpdt, MPI_BYTE, id->win);
        if(ret != MPI_SUCCESS)
        {
          fprintf(stderr, " Error in MPI_Put Line %d File %s\n", __LINE__, __FILE__);
          return PIDX_err_agg;
        }
      }
      else
      {
        printf("[3] TR %d (%d %d)\n", target_rank + ab->aggregator_interval, target_rank, ab->aggregator_interval);
        ret = MPI_Get(hz_buffer + (((samples_in_file / ab->agg_f) - target_disp) + ((end_agg_index - start_agg_index - 1) * (samples_in_file / ab->agg_f))) * bpdt, (target_count - (((end_agg_index - start_agg_index - 1) * ((samples_in_file / ab->agg_f))) + (((samples_in_file / ab->agg_f)) - target_disp))) * bpdt, MPI_BYTE, target_rank + ab->aggregator_interval, 0, (target_count - ((end_agg_index - start_agg_index) * (samples_in_file / ab->agg_f) - target_disp)) * bpdt, MPI_BYTE, id->win);
        if(ret != MPI_SUCCESS)
        {
          fprintf(stderr, " Error in MPI_Get Line %d File %s\n", __LINE__, __FILE__);
          return PIDX_err_agg;
        }
      }
#ifndef PIDX_ACTIVE_TARGET
      MPI_Win_unlock(target_rank + ab->aggregator_interval, id->win);
#endif
    }
    else
    {
      if(MODE == PIDX_WRITE)
      {
        memcpy( ab->buffer, hz_buffer + (((samples_in_file / ab->agg_f) - target_disp) + ((end_agg_index - start_agg_index - 1) * (samples_in_file / ab->agg_f))) * bpdt, (target_count - ((end_agg_index - start_agg_index) * (samples_in_file / ab->agg_f) - target_disp)) * bpdt);
      }
      else
      {
        memcpy( hz_buffer + (((samples_in_file / ab->agg_f) - target_disp) + ((end_agg_index - start_agg_index - 1) * (samples_in_file / ab->agg_f))) * bpdt, ab->buffer, (target_count - ((end_agg_index - start_agg_index) * (samples_in_file / ab->agg_f) - target_disp)) * bpdt);
      }
    }
  }
  //{
  //  printf("read vs write conflict!\n");
  //}
  else
  {
    if(target_rank != id->idx_c->lrank)
    {
#if PIDX_HAVE_MPI
#ifndef PIDX_ACTIVE_TARGET
      MPI_Win_lock(MPI_LOCK_SHARED, target_rank, 0 , id->win);
#endif
      //target_disp_address = target_disp;
      if(MODE == PIDX_WRITE)
      {
#ifdef PIDX_DUMP_AGG
        if (id->idx_d->dump_agg_info == 1 && id->idx->current_time_step == 0)
        {
          fprintf(agg_dump_fp, "[D] Target Rank %d Count %lld Local Disp %d Target Disp %lld\n", target_rank,  (long long)hz_count, 0, (long long)target_disp);
          fflush(agg_dump_fp);
        }
#endif

        //if (id->idx_c->grank == 0)
        //  printf("Size %d Target rank %d Target disp %d\n", hz_count, target_rank, target_disp);
        ret = MPI_Put(hz_buffer, hz_count * vps * bpdt, MPI_BYTE, target_rank, target_disp, hz_count * vps * bpdt, MPI_BYTE, id->win);
        if(ret != MPI_SUCCESS)
        {
          fprintf(stderr, " Error in MPI_Put Line %d File %s\n", __LINE__, __FILE__);
          return PIDX_err_agg;
        }
      }
      else
      {
#ifdef PIDX_DUMP_AGG
        if (id->idx_d->dump_agg_info == 1 && id->idx->current_time_step == 0)
        {
          fprintf(agg_dump_fp, "[D] Target Rank %d Count %lld Local Disp %d Target Disp %lld\n", target_rank,  (long long)hz_count, 0, (long long)target_disp);
          fflush(agg_dump_fp);
        }
#endif

        //if (id->idx_c->grank == 0)
        //  printf("Size %d Target rank %d Target disp %d\n", hz_count, target_rank, target_disp);
        ret = MPI_Get(hz_buffer, hz_count * vps * bpdt, MPI_BYTE, target_rank, target_disp, hz_count * vps * bpdt, MPI_BYTE, id->win);
        if(ret != MPI_SUCCESS)
        {
          fprintf(stderr, " Error in MPI_Put Line %d File %s\n", __LINE__, __FILE__);
          return PIDX_err_agg;
        }
      }
#ifndef PIDX_ACTIVE_TARGET
      MPI_Win_unlock(target_rank, id->win);
#endif
#endif
    }
    else
    {
      if(MODE == PIDX_WRITE)
      {

#ifdef PIDX_DUMP_AGG
        if (id->idx_d->dump_agg_info == 1 && id->idx->current_time_step == 0)
        {
          fprintf(agg_dump_fp, "[MD] Count %lld Local Disp %d Target Disp %lld\n", (long long)hz_count, 0, (long long)target_disp);
          fflush(agg_dump_fp);
        }
#endif
        memcpy( ab->buffer + target_disp * bpdt, hz_buffer, hz_count * vps * bpdt);
      }
      else
      {
#ifdef PIDX_DUMP_AGG
        if (id->idx_d->dump_agg_info == 1 && id->idx->current_time_step == 0)
        {
          fprintf(agg_dump_fp, "[MD] Count %lld Local Disp %d Target Disp %lld\n", (long long)hz_count, 0, (long long)target_disp);
          fflush(agg_dump_fp);
        }
#endif
        memcpy( hz_buffer, ab->buffer + target_disp * bpdt, hz_count * vps * bpdt);
      }
    }
  }

  return PIDX_success;
}


static PIDX_return_code compressed_aggregate(PIDX_agg_id id, int variable_index, unsigned long long hz_start, unsigned long long hz_count, unsigned char* hz_buffer, int buffer_offset, Agg_buffer ab, PIDX_block_layout lbl, int MODE)
{
  int ret;
  int bpdt;
  int file_no = 0, block_no = 0, negative_block_offset = 0, sample_index = 0, vps;
  int target_rank = 0;
  unsigned long long target_disp = 0, samples_in_file = 0;
  unsigned long long samples_per_file = (unsigned long long) id->idx_d->samples_per_block * id->idx->blocks_per_file;

  unsigned long long tcs = id->idx->chunk_size[0] * id->idx->chunk_size[1] * id->idx->chunk_size[2];


  PIDX_variable_group var_grp = id->idx->variable_grp[id->gi];
  PIDX_variable var = var_grp->variable[variable_index];

  vps = var->vps; //number of samples for variable j

  // hz_start is the starting HZ index for the data buffer at level "level" and for regular patch number "patch"
  //file number to which the first element of the buffer belongs to
  file_no = hz_start / samples_per_file;

  //block number for the first element of the buffer
  block_no = hz_start / id->idx_d->samples_per_block;


  //number of empty blocks befor block "block_no" in the file "file_no"
  negative_block_offset = PIDX_blocks_find_negative_offset(id->idx->blocks_per_file, block_no, lbl);
  if (negative_block_offset < 0)
    return PIDX_err_agg;

  //number of samples in file "file_no"
  //samples_in_file = id->idx->variable[id->ini]->bcpf[file_no] * id->idx_d->samples_per_block;
  samples_in_file = lbl->bcpf[file_no] * id->idx_d->samples_per_block;
  if (samples_in_file > samples_per_file)
    return PIDX_err_agg;

  //Calculating the hz index of "hz_start" relative to the file to which it belongs also taking into account empty blocks in file
  target_disp = ((hz_start - ((samples_per_file * file_no) + (negative_block_offset * id->idx_d->samples_per_block))) * vps)
      %
      (samples_in_file * vps);

  sample_index = target_disp / (samples_in_file / ab->agg_f);
  if (sample_index >= var->vps * ab->agg_f)
    return PIDX_err_agg;

  target_disp = target_disp % (samples_in_file / ab->agg_f);
  target_rank = id->agg_r[lbl->inverse_existing_file_index[file_no]][variable_index - id->fi][sample_index];
  bpdt = ((var->bpv / 8) * tcs) / (id->idx->compression_factor);
  hz_buffer = hz_buffer + buffer_offset * bpdt * vps;


  if(target_rank != id->idx_c->lrank)
  {
#if PIDX_HAVE_MPI
#ifndef PIDX_ACTIVE_TARGET
    MPI_Win_lock(MPI_LOCK_SHARED, target_rank, 0 , id->win);
#endif
    ret = MPI_Put(hz_buffer, hz_count, MPI_BYTE, target_rank, target_disp, hz_count, MPI_BYTE, id->win);
    if(ret != MPI_SUCCESS)
    {
      fprintf(stderr, " Error in MPI_Put Line %d File %s\n", __LINE__, __FILE__);
      return PIDX_err_agg;
    }
#ifndef PIDX_ACTIVE_TARGET
    MPI_Win_unlock(target_rank, id->win);
#endif
#endif
  }
  else
    memcpy( ab->buffer + target_disp * bpdt, hz_buffer, hz_count);



  return PIDX_success;
}


static PIDX_return_code report_error(PIDX_return_code ret, char* file, int line)
{
  fprintf(stdout,"File %s Line %d\n", file, line);
  return ret;
}


/// Function to check if NDimensional data chunks A and B intersects
static int intersectNDChunk(Ndim_patch A, Ndim_patch B)
{
  int d = 0, check_bit = 0;
  for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
    check_bit = check_bit || (A->offset[d] + A->size[d] - 1) < B->offset[d] || (B->offset[d] + B->size[d] - 1) < A->offset[d];

  return !(check_bit);
}
