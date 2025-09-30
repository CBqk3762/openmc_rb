#!/usr/bin/env bash

source /home/ciara-byers/Projects/openmc_rb/mc_Pyenv/bin/activate
export DAGMC_DIR=/home/ciara-byers/Projects/aegis/aegis-deps/DAGMC/bld/
export LD_LIBRARY_PATH=/home/ciara-byers/Projects/aegis/aegis-deps/EMBREE/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/hdf5/serial/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/home/ciara-byers/Projects/aegis/aegis-deps/MOAB/bin:$LD_LIBRARY_PATH
export OPENMC_CROSS_SECTIONS=/home/ciara-byers/Projects/crosssecs/endfb-vii.1-hdf5/cross_sections.xml
export OMP_NUM_THREADS=1
export OPENMC_USE_DAGMC=on
export CMAKE_INSTALL_PREFIX=/home/ciara-byers/Projects/openmc/openmc
export HDF5_DIR=/usr/bin
export DOPENMC_USE_MPI=off 
