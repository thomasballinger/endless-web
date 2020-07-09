EMSCRIPTEN_ENV := $(shell command -v emmake 2> /dev/null)

dataversion.js: endless-sky.js
clean:
	rm -f endless-sky.js
	rm -f endless-sky.worker.js
	rm -f endless-sky.data
	rm -f endless-sky.wasm
	rm -f dataversion.js
	rm -rf output
	rm -f endless-sky.wasm.map
	rm -f lib/emcc/libendless-sky.a
clean-full: clean
	rm -f favicon.ico
	rm -f Ubuntu-Regular.ttf
	rm -f title.png
	rm -rf build/emcc
2.1.0.tar.gz:
	wget https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/2.1.0.tar.gz
libjpeg-turbo-2.1.0: 2.1.0.tar.gz
	tar xzvf 2.1.0.tar.gz
# | means libjpeg-turbo-2.1.0 is a "order-only prerequisite" so creating the file doesn't invalidate the dir
libjpeg-turbo-2.1.0/libturbojpeg.a: | libjpeg-turbo-2.1.0
ifndef EMSCRIPTEN_ENV
	$(error "emmake is not available, activate the emscripten env first")
endif
	cd libjpeg-turbo-2.1.0; emcmake cmake -G"Unix Makefiles" -DWITH_SIMD=0 -DCMAKE_BUILD_TYPE=Release -Wno-dev
	cd libjpeg-turbo-2.1.0; emmake make
dev: endless-sky.js dataversion.js Ubuntu-Regular.ttf title.png
	emrun --serve_after_close --serve_after_exit --browser chrome --private_browsing endless-sky.html
title.png:
	cp images/_menu/title.png title.png
Ubuntu-Regular.ttf:
	curl -Ls 'https://github.com/google/fonts/blob/main/ufl/ubuntu/Ubuntu-Regular.ttf?raw=true' > Ubuntu-Regular.ttf
favicon.ico:
	wget https://endless-sky.github.io/favicon.ico

COMMON_FLAGS = -O3 -flto\
		-s USE_SDL=2\
		-s USE_LIBPNG=1\
		-s DISABLE_EXCEPTION_CATCHING=0

CFLAGS = $(COMMON_FLAGS)\
	-Duuid_generate_random=uuid_generate\
	-std=c++11\
	-Wall\
	-Werror\
	-Wold-style-cast\
	-DES_GLES\
	-DES_NO_THREADS\
	-gsource-map\
	-fno-rtti\
	-I libjpeg-turbo-2.1.0\


# Note that that libmad is not linked! It's mocked out until it works with Emscripten
LINK_FLAGS = $(COMMON_FLAGS)\
	-L libjpeg-turbo-2.1.0\
	-l jpeg\
	-lidbfs.js\
	--source-map-base http://localhost:6931/\
	-s USE_WEBGL2=1\
	-s ASSERTIONS=2\
	-s DEMANGLE_SUPPORT=1\
	-s GL_ASSERTIONS=1\
	--closure 1\
	-s ASYNCIFY\
	-s MIN_WEBGL_VERSION=2\
	-s MAX_WEBGL_VERSION=2\
	-s WASM_MEM_MAX=2147483648\
	-s INITIAL_MEMORY=629145600\
	-s ALLOW_MEMORY_GROWTH=1\
	--preload-file data\
	--preload-file images\
	--preload-file sounds\
	--preload-file credits.txt\
	--preload-file keys.txt\
	-s EXPORTED_RUNTIME_METHODS=['callMain']\
	--emrun

CPPS := $(shell ls source/*.cpp) $(shell ls source/text/*.cpp)
CPPS_EXCEPT_MAIN := $(shell ls source/*.cpp | grep -v main.cpp) $(shell ls source/text/*.cpp)
TEMP := $(subst source/,build/emcc/,$(CPPS))
OBJS := $(subst .cpp,.o,$(TEMP))
TEMP := $(subst source/,build/emcc/,$(CPPS_EXCEPT_MAIN))
OBJS_EXCEPT_MAIN := $(subst .cpp,.o,$(TEMP))
HEADERS := $(shell ls source/*.h*) $(shell ls source/text/*.h*)

build/emcc/%.o: source/%.cpp
	@mkdir -p build/emcc
	@mkdir -p build/emcc/text
	em++ $(CFLAGS) -c $< -o $@

lib/emcc/libendless-sky.a: $(OBJS_EXCEPT_MAIN)
	@mkdir -p lib/emcc
	emar rcs lib/emcc/libendless-sky.a $(OBJS_EXCEPT_MAIN)

endless-sky.js: libjpeg-turbo-2.1.0/libturbojpeg.a lib/emcc/libendless-sky.a build/emcc/main.o
ifndef EMSCRIPTEN_ENV
	$(error "em++ is not available, activate the emscripten env first")
endif
	em++ -o endless-sky.js $(LINK_FLAGS) build/emcc/main.o lib/emcc/libendless-sky.a

dataversion.js: endless-sky.js
	./hash-data.py endless-sky.data dataversion.js
output/index.html: endless-sky.js endless-sky.html favicon.ico endless-sky.data Ubuntu-Regular.ttf dataversion.js
	rm -rf output
	mkdir -p output
	cp endless-sky.html output/index.html
	cp endless-sky.wasm endless-sky.data endless-sky.js endless-sky.worker.js output/
	cp -r js/ output/js
	cp dataversion.js output/
	cp loading.mp3 output/
	cp favicon.ico output/
	cp Ubuntu-Regular.ttf output/
deploy: output/index.html
	@if curl -s https://play-endless-sky.com/dataversion.js | diff - dataversion.js; \
		then \
			echo 'uploading all files except endless-sky.data...'; \
			aws s3 sync --exclude endless-sky.data output s3://play-endless-sky.com/live;\
		else \
			echo 'uploading all files, including endless-sky.data...'; \
			aws s3 sync output s3://play-endless-sky.com/live;\
	fi
	# play-endless-sky.com
	aws cloudfront create-invalidation --distribution-id E2TZUW922XPLEF --paths /\*
	# play-endless-web.com
	aws cloudfront create-invalidation --distribution-id E3D0Y4DMGSVPWC --paths /\*
