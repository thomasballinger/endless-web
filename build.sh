#!/bin/bash
# build.sh builds a web build for an Endless Sky commit
set -ex

# Require to do merges
git config --global user.email "example@example.com"
git config --global user.name "beep boop"

echo Was this triggered by a webhook?
echo INCOMING_HOOK_TITLE $INCOMING_HOOK_TITLE
echo INCOMING_HOOK_TITLE $INCOMING_HOOK_BODY
body_size=${#INCOMING_HOOK_BODY}
if [[ ${#INCOMING_HOOK_TITLE} == "0" ]]; then
  echo Not a hook
elif [[ ${#INCOMING_HOOK_BODY} != "40" ]]; then
  echo Bad hook response body! Submit a git sha.
else
  echo Deploy hook called with sha $INCOMING_HOOK_BODY
  # check out  emscripten changes
  git fetch https://github.com/endless-sky/endless-sky.git $INCOMING_HOOK_BODY
  git checkout FETCH_HEAD
  echo "Let's hope the web build changes merge cleanly into this!"
fi

# merge in emscripten changes
git fetch https://github.com/thomasballinger/endless-sky.git es-wasm-reformatted
git merge FETCH_HEAD

# install build system used by Endless Sky (it's not popular enough to already be installed)
python -m pip install scons

# activate emscripten (built by plugin)
EMSCRIPTEN_VERSION="2.0.16" # last one that works before uniform shader issues

# check caches
ls build/emcc || true
ls lib/emcc || true
ls emsdk/upstream/emscripten/cache || true
ls emsdk/upstream/emscripten/cache/wasm-lto/ || true

./emsdk/emsdk activate $EMSCRIPTEN_VERSION || exit $?
source emsdk/emsdk_env.sh

#echo "Checking if cache has been cleared?"
ls emsdk/upstream/emscripten/cache
ls emsdk/upstream/emscripten/cache/wasm-lto/

make output/index.html || exit $?

#echo "After making, what's the cache look like?"
#ls ./emsdk/upstream/emscripten/cache/
#ls ./emsdk/upstream/emscripten/cache/wasm-lto/

echo Deploy url:
echo $DEPLOY_URL
