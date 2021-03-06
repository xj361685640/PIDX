#ifndef __PIDX_WAVELET_IDX_STENCIL_H
#define __PIDX_WAVELET_IDX_STENCIL_H


///
/// \brief wavelet
/// \param file
/// \param gi
/// \param svi
/// \param evi
/// \return
///
PIDX_return_code idx_stencil_wavelet(PIDX_io file, int gi, int svi, int evi, int mode);



///
/// \brief compute_average
/// \param file
/// \param gi
/// \param svi
/// \param evi
/// \param mode
/// \return
///
PIDX_return_code compute_average(PIDX_io file, int gi, int svi, int evi, int mode);


#endif

