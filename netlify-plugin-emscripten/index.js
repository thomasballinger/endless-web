const fs = require("fs");

module.exports = {
  async onPreBuild({ utils, inputs }) {
    // build it
    if (fs.existsSync("./emsdk")) {
      console.log("emsdk already exists?! Does Netlify cache this stuff?");
      console.log(fs.readdirSync("./emsdk"));
    } else {
      await utils.run("git", [
        "clone",
        "https://github.com/emscripten-core/emsdk.git",
      ]);
    }
    await utils.run("./emsdk/emsdk", ["install", inputs.version]);

    console.log(
      `emsdk installed! You still need to activate (run "emsdk/emsdk activate ${inputs.version}") emscripten in your build.`
    );
  },
};

// TODO in the post-build, cache the cache folder and in the pre-build restore it
