EMSCRIPTEN_ENV := $(shell command -v emmake 2> /dev/null)
CXX := $(shell command -v ccache 2> /dev/null > /dev/null && echo ccache em++ || echo em++)

all: dev
clean:
	rm -f endless-sky.js
	rm -f endless-sky.data
	rm -f endless-sky.wasm
	rm -f dataversion.js
	rm -rf output
	rm -f endless-sky.wasm.map
	rm -f lib/emcc/libendless-sky.a
	rm -f favicon.ico
	rm -f Ubuntu-Regular.ttf
	rm -f title.png
	rm -rf build/emcc
distclean: clean
	rm -rf lib/emcc
	rm -rf libjpeg-turbo-2.1.0
2.1.0.tar.gz:
	wget -nv https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/2.1.0.tar.gz
libjpeg-turbo-2.1.0: 2.1.0.tar.gz
	tar xzf 2.1.0.tar.gz
libjpeg-turbo-2.1.0/libturbojpeg.a: | libjpeg-turbo-2.1.0
ifndef EMSCRIPTEN_ENV
	$(error "emmake is not available, activate the emscripten env first")
endif
	cd libjpeg-turbo-2.1.0; emcmake cmake -G"Unix Makefiles" -DWITH_SIMD=0 -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev
	cd libjpeg-turbo-2.1.0; sed 's/SIZEOF_SIZE_T  [0-9]*/SIZEOF_SIZE_T  4/' jconfigint.h > jconfigint.h.tmp && mv jconfigint.h.tmp jconfigint.h
	cd libjpeg-turbo-2.1.0; emmake $(MAKE)
dev: endless-sky.js dataversion.js Ubuntu-Regular.ttf title.png
	emrun --serve_after_close --serve_after_exit --browser chrome --private_browsing endless-sky.html
title.png:
	cp images/_menu/title.png title.png
Ubuntu-Regular.ttf:
	curl -Ls 'https://github.com/google/fonts/blob/main/ufl/ubuntu/Ubuntu-Regular.ttf?raw=true' > Ubuntu-Regular.ttf
favicon.ico:
	wget -nv https://endless-sky.github.io/favicon.ico

COMMON_FLAGS = -O3\
		-s USE_SDL=2\
		-s USE_LIBPNG=1\
		-s DISABLE_EXCEPTION_CATCHING=0

CFLAGS = $(COMMON_FLAGS)\
	-s USE_ZLIB=1\
	-Duuid_generate_random=uuid_generate\
	-std=c++20\
	-Wall\
	-Werror\
	-Wold-style-cast\
	-DES_GLES\
	-gsource-map\
	-I libjpeg-turbo-2.1.0\

LINK_FLAGS = $(COMMON_FLAGS)\
	-s LLD_REPORT_UNDEFINED\
	-s USE_ZLIB=1\
	-L libjpeg-turbo-2.1.0\
	-l jpeg\
	-lopenal\
	-lidbfs.js\
	--source-map-base http://localhost:6931/\
	-s USE_WEBGL2=1\
	-s ASSERTIONS=2\
	-s GL_ASSERTIONS=1\
	-s ASYNCIFY\
	-s MIN_WEBGL_VERSION=2\
	-s MAX_WEBGL_VERSION=2\
	-s WASM_MEM_MAX=2147483648\
	-s INITIAL_MEMORY=1347289088\
	-s ALLOW_MEMORY_GROWTH=1\
	--preload-file data\
	--preload-file images\
	--preload-file sounds\
	--preload-file shaders\
	--preload-file credits.txt\
	--preload-file keys.txt\
	-s EXPORTED_RUNTIME_METHODS=['callMain']\
	--emrun

# Source files: all .cpp in source subdirs except test/, windows/, and excluded audio suppliers
# Also exclude ZipFile.cpp (no minizip in emscripten)
CPPS := $(filter-out source/ZipFile.cpp,$(shell ls source/*.cpp)) \
	$(shell ls source/text/*.cpp) \
	$(shell ls source/ship/*.cpp) \
	$(shell ls source/audio/*.cpp) \
	$(shell ls source/audio/player/*.cpp) \
	$(shell ls source/audio/supplier/AudioSupplier.cpp source/audio/supplier/WavSupplier.cpp source/audio/supplier/effect/Fade.cpp) \
	$(shell ls source/image/*.cpp) \
	$(shell ls source/shader/*.cpp) \
	$(shell ls source/comparators/*.cpp 2>/dev/null) \
	$(shell ls source/orders/*.cpp 2>/dev/null) \
	$(shell ls source/test/*.cpp 2>/dev/null)
CPPS_EXCEPT_MAIN := $(filter-out source/main.cpp,$(CPPS))
TEMP := $(subst source/,build/emcc/,$(CPPS))
OBJS := $(subst .cpp,.o,$(TEMP))
TEMP := $(subst source/,build/emcc/,$(CPPS_EXCEPT_MAIN))
OBJS_EXCEPT_MAIN := $(subst .cpp,.o,$(TEMP))
HEADERS := $(shell find source -name '*.h' -o -name '*.hpp' | grep -v windows/)

BUILD_DIRS := build/emcc build/emcc/text build/emcc/ship build/emcc/audio \
	build/emcc/audio/player build/emcc/audio/supplier build/emcc/audio/supplier/effect \
	build/emcc/image build/emcc/shader build/emcc/comparators build/emcc/orders build/emcc/test

build/emcc/%.o: source/%.cpp $(HEADERS) libjpeg-turbo-2.1.0/libturbojpeg.a
	@mkdir -p $(BUILD_DIRS)
	$(CXX) $(CFLAGS) -c $< -o $@

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
output/index.html: endless-sky.js endless-sky.html favicon.ico Ubuntu-Regular.ttf dataversion.js js/cached-resource.js js/plugins.js js/save-games.js
	rm -rf output
	mkdir -p output
	cp endless-sky.html to-be-modified-endless-sky.html
	cp endless-sky.js to-be-modified-endless-sky.js
	./copy-to-hashed-location.py endless-sky.wasm endless-sky.data endless-sky.js output/
	mkdir output/js
	./copy-to-hashed-location.py js/* output/
	./copy-to-hashed-location.py dataversion.js output/
	./copy-to-hashed-location.py loading.mp3 output/
	./copy-to-hashed-location.py Ubuntu-Regular.ttf output/
	cp favicon.ico output/
	mv to-be-modified-endless-sky.js output/endless-sky-*.js
	mv to-be-modified-endless-sky.html output/index.html
test: output/index.html
	cd output; emrun --serve_after_close --serve_after_exit --browser chrome --private_browsing index.html
deploy: output/index.html
	aws s3 sync --size-only --exclude index.html output s3://play-endless-sky.com/live --cache-control 'public, max-age=604800, immutable'
	aws s3 sync --exclude '*' --include index.html output s3://play-endless-sky.com/live --cache-control 'max-age=0'
	aws cloudfront create-invalidation --distribution-id E2TZUW922XPLEF --paths / /index.html
	aws cloudfront create-invalidation --distribution-id E3D0Y4DMGSVPWC --paths / /index.html
