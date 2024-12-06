# cmake -S . -B build && pushd build && make -j 16
cmake -S . -B build -DPICO_COPY_TO_RAM=1 -DPICO_BOARD=pico2 && pushd build && make -j 16
