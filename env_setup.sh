#!/usr/bin/env bash

source /home/cbyers/Projects/openmc/mc_Pyenv/bin/activate
export DAGMC_DIR=/home/cbyers/Projects/dagmc_bld/aegis-deps/DAGMC/bld/
export LD_LIBRARY_PATH=/home/cbyers/Projects/dagmc_bld/aegis-deps/EMBREE/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu/hdf5/serial/lib:$LD_LIBRARY_PATH
export PYTHONPATH=/home/cbyers/Projects/dagmc_bld/aegis-deps/MOAB/lib/python3.12/site-packages:$PYTHONPATH
export OPENMC_CROSS_SECTIONS=/home/cbyers/Projects/crosssecs/endfb-vii.1-hdf5/cross_sections.xml
