set -v

cd n64_rom
./build.sh
cd ..

cd wasm_model64
./build.sh
cd ..
