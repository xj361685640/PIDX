/* decompress 1d contiguous array */
static void
_t2(decompress, Scalar, 1)(zfp_stream* stream, zfp_field* field)
{
  Scalar* data = field->data;
  uint nx = field->nx;
  uint mx = nx & ~3u;
  uint x;

  for (x = 0; x < mx; x += 4, data += 4)
    _t2(zfp_decode_block, Scalar, 1)(stream, data);
  if (x < nx)
    _t2(zfp_decode_partial_block_strided, Scalar, 1)(stream, data, nx - x, 1);
}

/* decompress 1d strided array */
static void
_t2(decompress_strided, Scalar, 1)(zfp_stream* stream, zfp_field* field)
{
  Scalar* data = field->data;
  uint nx = field->nx;
  uint mx = nx & ~3u;
  int sx = field->sx;
  uint x;

  for (x = 0; x < mx; x += 4, data += 4 * sx)
    _t2(zfp_decode_block_strided, Scalar, 1)(stream, data, sx);
  if (x < nx)
    _t2(zfp_decode_partial_block_strided, Scalar, 1)(stream, data, nx - x, sx);
}

/* decompress 2d strided array */
static void
_t2(decompress_strided, Scalar, 2)(zfp_stream* stream, zfp_field* field)
{
  Scalar* data = field->data;
  uint nx = field->nx;
  uint ny = field->ny;
  uint mx = nx & ~3u;
  uint my = ny & ~3u;
  int sx = field->sx ? field->sx : 1;
  int sy = field->sy ? field->sy : nx;
  uint x, y;

  for (y = 0; y < my; y += 4, data += 4 * sy - mx * sx) {
    for (x = 0; x < mx; x += 4, data += 4 * sx)
      _t2(zfp_decode_block_strided, Scalar, 2)(stream, data, sx, sy);
    if (x < nx)
      _t2(zfp_decode_partial_block_strided, Scalar, 2)(stream, data, nx - x, 4, sx, sy);
  }
  if (y < ny) {
    for (x = 0; x < mx; x += 4, data += 4 * sx)
      _t2(zfp_decode_partial_block_strided, Scalar, 2)(stream, data, 4, ny - y, sx, sy);
    if (x < nx)
      _t2(zfp_decode_partial_block_strided, Scalar, 2)(stream, data, nx - x, ny - y, sx, sy);
  }
}

/* decompress 3d strided array */
static void
_t2(decompress_strided, Scalar, 3)(zfp_stream* stream, zfp_field* field)
{
  Scalar* data = field->data;
  uint nx = field->nx;
  uint ny = field->ny;
  uint nz = field->nz;
  uint mx = nx & ~3u;
  uint my = ny & ~3u;
  uint mz = nz & ~3u;
  int sx = field->sx ? field->sx : 1;
  int sy = field->sy ? field->sy : nx;
  int sz = field->sz ? field->sz : nx * ny;
  uint x, y, z;

  for (z = 0; z < mz; z += 4, data += 4 * sz - my * sy) {
    for (y = 0; y < my; y += 4, data += 4 * sy - mx * sx) {
      for (x = 0; x < mx; x += 4, data += 4 * sx)
        _t2(zfp_decode_block_strided, Scalar, 3)(stream, data, sx, sy, sz);
      if (x < nx)
        _t2(zfp_decode_partial_block_strided, Scalar, 3)(stream, data, nx - x, 4, 4, sx, sy, sz);
    }
    if (y < ny) {
      for (x = 0; x < mx; x += 4, data += 4 * sx)
        _t2(zfp_decode_partial_block_strided, Scalar, 3)(stream, data, 4, ny - y, 4, sx, sy, sz);
      if (x < nx)
        _t2(zfp_decode_partial_block_strided, Scalar, 3)(stream, data, nx - x, ny - y, 4, sx, sy, sz);
      data -= mx * sx;
    }
  }
  if (z < nz) {
    for (y = 0; y < my; y += 4, data += 4 * sy - mx * sx) {
      for (x = 0; x < mx; x += 4, data += 4 * sx)
        _t2(zfp_decode_partial_block_strided, Scalar, 3)(stream, data, 4, 4, nz - z, sx, sy, sz);
      if (x < nx)
        _t2(zfp_decode_partial_block_strided, Scalar, 3)(stream, data, nx - x, 4, nz - z, sx, sy, sz);
    }
    if (y < ny) {
      for (x = 0; x < mx; x += 4, data += 4 * sx)
        _t2(zfp_decode_partial_block_strided, Scalar, 3)(stream, data, 4, ny - y, nz - z, sx, sy, sz);
      if (x < nx)
        _t2(zfp_decode_partial_block_strided, Scalar, 3)(stream, data, nx - x, ny - y, nz - z, sx, sy, sz);
    }
  }
}
/* decompress 3d strided array */
static void
_t2(decompress_strided2, Scalar, 3)(zfp_stream* stream, zfp_field* field, Int* data, int* emax)
{
  uint nx = field->nx;
  uint ny = field->ny;
  uint nz = field->nz;
  uint mx = nx & ~3u;
  uint my = ny & ~3u;
  uint mz = nz & ~3u;
  int sx = field->sx ? field->sx : 1;
  int sy = field->sy ? field->sy : nx;
  int sz = field->sz ? field->sz : nx * ny;
  uint x, y, z;

  for (z = 0; z < mz; z += 4, data += 4 * sz - my * sy) {
    for (y = 0; y < my; y += 4, data += 4 * sy - mx * sx) {
      for (x = 0; x < mx; x += 4, data += 4 * sx)
        _t2(zfp_decode_block_strided2, Scalar, 3)(stream, data, sx, sy, sz, emax++);
      if (x < nx)
        _t2(zfp_decode_partial_block_strided2, Scalar, 3)(stream, data, nx - x, 4, 4, sx, sy, sz, emax++);
    }
    if (y < ny) {
      for (x = 0; x < mx; x += 4, data += 4 * sx)
        _t2(zfp_decode_partial_block_strided2, Scalar, 3)(stream, data, 4, ny - y, 4, sx, sy, sz, emax++);
      if (x < nx)
        _t2(zfp_decode_partial_block_strided2, Scalar, 3)(stream, data, nx - x, ny - y, 4, sx, sy, sz, emax++);
      data -= mx * sx;
    }
  }
  if (z < nz) {
    for (y = 0; y < my; y += 4, data += 4 * sy - mx * sx) {
      for (x = 0; x < mx; x += 4, data += 4 * sx)
        _t2(zfp_decode_partial_block_strided2, Scalar, 3)(stream, data, 4, 4, nz - z, sx, sy, sz, emax++);
      if (x < nx)
        _t2(zfp_decode_partial_block_strided2, Scalar, 3)(stream, data, nx - x, 4, nz - z, sx, sy, sz, emax++);
    }
    if (y < ny) {
      for (x = 0; x < mx; x += 4, data += 4 * sx)
        _t2(zfp_decode_partial_block_strided2, Scalar, 3)(stream, data, 4, ny - y, nz - z, sx, sy, sz, emax++);
      if (x < nx)
        _t2(zfp_decode_partial_block_strided2, Scalar, 3)(stream, data, nx - x, ny - y, nz - z, sx, sy, sz, emax++);
    }
  }  
}
