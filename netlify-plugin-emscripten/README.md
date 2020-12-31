# Netlify Build Plugin: Persist Emscripten between builds

Do you use [Emscripten](https://emscripten.org/) in your Netlify project? Instead of building from scratch, cache the build with this plugin.

## Usage

You can also install it manually using `netlify.toml`. If you want to know more about file-based configuration on Netlify, click [here](https://docs.netlify.com/configure-builds/file-based-configuration/).

Add the following lines to your project's `netlify.toml` file:

```toml
[[plugins]]
  package = "netlify-plugin-emscripten-cache"
```

inspired by the GitHub Emscripten action
https://github.com/mymindstorm/setup-emsdk
