# cmake -S . -B build && pushd build && make -j 16
cmake -S . -B build -DPICO_BOARD=dato_submarine && pushd build && make -j 16
