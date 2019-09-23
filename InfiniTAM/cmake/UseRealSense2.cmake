#######################
# UseRealSense2.cmake #
#######################

OPTION(WITH_REALSENSE2 "Build with Intel RealSense SDK 2 support?" OFF)

IF(WITH_REALSENSE2)
#  FIND_PACKAGE(RealSense2 REQUIRED)
  find_package(realsense2 REQUIRED)
  set(RealSense2_LIBRARY ${realsense2_LIBRARY})
  INCLUDE_DIRECTORIES(${RealSense2_INCLUDE_DIR})
  ADD_DEFINITIONS(-DCOMPILE_WITH_RealSense2)
ENDIF()
