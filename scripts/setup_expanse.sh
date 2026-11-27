#!/bin/bash
# One-time setup: installs plantri, Regina and this project under $HOME/knots.
# Run on a login node from the repo folder: bash scripts/setup_expanse.sh
set -e

PREFIX=$HOME/knots
REPO=$(pwd)
JOBS=${JOBS:-8}
mkdir -p $PREFIX/src $PREFIX/bin

if command -v module > /dev/null 2>&1; then
    module purge
    module load cpu gcc openmpi cmake
fi

# Libraries Regina needs, without root access.
if [ ! -x $PREFIX/bin/micromamba ]; then
    cd $PREFIX
    curl -Ls https://micro.mamba.pm/api/micromamba/linux-64/latest | tar -xj bin/micromamba
fi
if [ ! -d $PREFIX/env ]; then
    $PREFIX/bin/micromamba create -y -p $PREFIX/env -c conda-forge gmp libxml2 libxml2-devel zlib lmdb
fi

# plantri
if [ ! -x $PREFIX/bin/plantri ]; then
    cd $PREFIX/src
    curl -sLO https://users.cecs.anu.edu.au/~bdm/plantri/plantri55.tar.gz
    tar xzf plantri55.tar.gz
    cd plantri55
    make plantri
    cp plantri $PREFIX/bin/
fi

# Regina engine only
if [ ! -x $PREFIX/regina/bin/regina-engine-config ]; then
    cd $PREFIX/src
    curl -sLO https://github.com/regina-normal/regina/releases/download/regina-7.4.1/regina-7.4.1.tar.gz
    tar xzf regina-7.4.1.tar.gz
    mkdir -p regina-7.4.1/build
    cd regina-7.4.1/build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$PREFIX/regina \
        -DCMAKE_PREFIX_PATH=$PREFIX/env -DDISABLE_GUI=1 -DDISABLE_PYTHON=1 -DDISABLE_UTILS=1 \
        -DBUILD_TESTING=OFF
    make -j$JOBS
    make install
fi

# This project
export PATH=$PREFIX/regina/bin:$PREFIX/bin:$PATH
export LD_LIBRARY_PATH=$PREFIX/regina/lib:$PREFIX/env/lib:$LD_LIBRARY_PATH
cd $REPO
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$JOBS
echo "Done. census is at $REPO/build/census"
