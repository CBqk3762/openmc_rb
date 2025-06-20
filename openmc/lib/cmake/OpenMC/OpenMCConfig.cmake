get_filename_component(OpenMC_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

find_package(fmt REQUIRED HINTS ${OpenMC_CMAKE_DIR}/../fmt)
find_package(gsl-lite REQUIRED HINTS ${OpenMC_CMAKE_DIR}/../gsl-lite)
find_package(pugixml REQUIRED HINTS ${OpenMC_CMAKE_DIR}/../pugixml)
find_package(xtl REQUIRED HINTS ${OpenMC_CMAKE_DIR}/../xtl)
find_package(xtensor REQUIRED HINTS ${OpenMC_CMAKE_DIR}/../xtensor)
if(on)
  find_package(DAGMC REQUIRED HINTS /home/cbyers/Projects/dagmc_bld/aegis-deps/DAGMC/bld)
endif()

if(OFF)
  include(FindPkgConfig)
  list(APPEND CMAKE_PREFIX_PATH )
  set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH True)
  pkg_check_modules(LIBMESH REQUIRED >=1.7.0 IMPORTED_TARGET)
endif()

find_package(PNG)

if(NOT TARGET OpenMC::libopenmc)
  include("${OpenMC_CMAKE_DIR}/OpenMCTargets.cmake")
endif()

if(off)
  find_package(MPI REQUIRED)
endif()
