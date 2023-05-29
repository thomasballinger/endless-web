## Building for the web:

Mac and Linux (Windows not supported):

Install Emscripten following the instructions at https://emscripten.org/docs/getting_started/downloads.html
Use the latest version and source the emsdk_env.sh file so you can run commands like emcc, em++ and emmake.
The last time I checked, this looked like:

```
  $ git clone https://github.com/emscripten-core/emsdk.git
  $ cd emsdk
  $ ./emsdk install 3.1.24
  $ ./emsdk activate 3.1.24
  $ source ./emsdk_env.sh  # you'll need to run this one each time you open a new terminal
```

Now back in the endless-sky repo directory run: (maybe you need to install make, wget, and tar first? I figure those should be everywhere already)

```
  $ make dev
```
