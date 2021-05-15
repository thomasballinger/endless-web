EMSCRIPTEN_ENV := $(shell command -v emmake 2> /dev/null)

endless-sky.js: libjpeg-turbo-2.1.0/libturbojpeg.a
ifndef EMSCRIPTEN_ENV
	$(error "em++ is not available, activate the emscripten env first")
endif
	scons -j 8 mode=emcc music=off opengl=gles threads=off
dataversion.js: endless-sky.js
clean:
	rm -f endless-sky.js
	rm -f endless-sky.worker.js
	rm -f endless-sky.data
	rm -f endless-sky.wasm
	rm -f dataversion.js
	rm -rf output
	rm -f endless-sky.wasm.map
clean-full: clean
	rm -f favicon.ico
	rm -f Ubuntu-Regular.ttf
	rm -f title.png
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
dev: endless-sky.js Ubuntu-Regular.ttf
	emrun --serve_after_close --serve_after_exit --browser chrome --private_browsing endless-sky.html
favicon.ico:
	wget https://endless-sky.github.io/favicon.ico
Ubuntu-Regular.ttf:
	curl -Ls 'https://github.com/google/fonts/blob/main/ufl/ubuntu/Ubuntu-Regular.ttf?raw=true' > Ubuntu-Regular.ttf
title.png:
	cp images/_menu/title.png title.png
output/index.html: endless-sky.js endless-sky.html favicon.ico endless-sky.data Ubuntu-Regular.ttf dataversion.js
	rm -rf output
	mkdir -p output
	cp endless-sky.html output/index.html
	cp endless-sky.wasm endless-sky.data endless-sky.js endless-sky.worker.js output/
	cp -r js/ output/js
	cp dataversion.js output/
	cp title.png output/
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
	aws cloudfront create-invalidation --distribution-id E2TZUW922XPLEF --paths /\*
