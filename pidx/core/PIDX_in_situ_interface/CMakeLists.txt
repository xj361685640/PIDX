#/*****************************************************
# **  PIDX Parallel I/O Library                      **
# **  Copyright (c) 2010-2014 University of Utah     **
# **  Scientific Computing and Imaging Institute     **
# **  72 S Central Campus Drive, Room 3750           **
# **  Salt Lake City, UT 84112                       **
# **                                                 **
# **  PIDX is licensed under the Creative Commons    **
# **  Attribution-NonCommercial-NoDerivatives 4.0    **
# **  International License. See LICENSE.md.         **
# **                                                 **
# **  For information about this project see:        **
# **  http://www.cedmav.com/pidx                     **
# **  or contact: pascucci@sci.utah.edu              **
# **  For support: PIDX-support@visus.net            **
# **                                                 **
# *****************************************************/
# ////////////////////////////////////////
# sources
# ////////////////////////////////////////

FILE(GLOB PIDX_IN_SITU_INTERFACE_SOURCES *.h *.c)


# ////////////////////////////////////////
# includes
# ////////////////////////////////////////

IF (MPI_C_FOUND)
  #MESSAGE("Configuring pidx with MPI support")
  INCLUDE_DIRECTORIES(${MPI_C_INCLUDE_PATH})
  SET(PIDX_IN_SITU_INTERFACE_LINK_LIBS ${PIDX_IN_SITU_INTERFACE_LINK_LIBS} ${MPI_C_LIBRARIES})
ENDIF()

IF (ZFP_FOUND)
  MESSAGE("Configuring pidx with ZFP support")
  INCLUDE_DIRECTORIES(${ZFP_INCLUDE_DIR})
  SET(PIDX_IN_SITU_INTERFACE_LINK_LIBS ${PIDX_IN_SITU_INTERFACE_LINK_LIBS} ${ZFP_LIBRARIES})
ENDIF()

IF (PMT_FOUND)
  MESSAGE("Configuring pidx with PMT support XXXX" ${PMT_INSITU_LINK_LIBRARIES} )
  INCLUDE_DIRECTORIES(${PMT_INSITU_INCLUDE_DIR})
  SET(PIDX_IN_SITU_INTERFACE_LINK_LIBS ${PIDX_IN_SITU_INTERFACE_LINK_LIBS} ${PMT_INSITU_LINK_LIBRARIES})
ENDIF()

IF (VTK_RENDERER_FOUND)
  MESSAGE("Configuring rendering support XXXX" ${VTK_RENDERER_INCLUDE_DIR} "AAAAAAA" ${VTK_RENDERER_LINK_LIBRARIES} "BBBBBB" )
  INCLUDE_DIRECTORIES(${VTK_RENDERER_INCLUDE_DIR})
  SET(PIDX_IN_SITU_INTERFACE_LINK_LIBS ${PIDX_IN_SITU_INTERFACE_LINK_LIBS} ${VTK_RENDERER_LINK_LIBRARIES})
ENDIF()


# ////////////////////////////////////////
# library
# ////////////////////////////////////////

PIDX_ADD_LIBRARY(pidx_in_situ_interface "${PIDX_IN_SITU_INTERFACE_SOURCES}")
TARGET_LINK_LIBRARIES(pidx_in_situ_interface m ${PIDX_IN_SITU_INTERFACE_LINK_LIBS})
SET_TARGET_PROPERTIES(pidx_in_situ_interface PROPERTIES LINKER_LANGUAGE CXX)
