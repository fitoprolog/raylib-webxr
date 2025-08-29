all:
	em++ -o game.html main.cpp -Os -Wall  ../raylib/src/libraylib.a \
		-I. -I ../raylib/src/  -s USE_GLFW=3 -s ASYNCIFY -s DYNCALLS \
		-DPLATFORM_WEB --preload-file resources/ --js-library library_webxr.js --profiling \
	        -s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap','setValue','getValue']" \
	        -s "EXPORTED_FUNCTIONS=['_malloc','_free','_main','_launchit']"
