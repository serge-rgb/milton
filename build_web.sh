# TOTAL_MEMORY = 256 * 1024 * 1024 + 1
emcc src/web_milton.c -s USE_SDL=2 -s TOTAL_MEMORY=268435457 -O2 -o build/HTML/web_milton.html
