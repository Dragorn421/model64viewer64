export EMSDK_QUIET=1
# setup emsdk: https://emscripten.org/docs/getting_started/downloads.html
source "/home/dragorn421/Documents/emsdk/emsdk_env.sh"
export EMSDK_QUIET=0

mkdir -p build
emcc main.c \
-o build/main.html \
--shell-file shell_minimal.html \
-s NO_EXIT_RUNTIME=1 -s "EXPORTED_RUNTIME_METHODS=['ccall']" \
--embed-file ../n64_rom/model64viewer64.z64@rom.z64
