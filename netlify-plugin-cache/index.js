module.exports = {
  async onPreBuild({ utils, inputs }) {
    // check cache for this build
    if (await utils.cache.restore(inputs.cache)) {
      console.log("Found stuff to restore for", inputs.cache);
    } else {
      console.log("Nothing found to restore for", inputs.cache);
    }
  },
  async onPostBuild({ utils, inputs }) {
    if (await utils.cache.save(inputs.cache)) {
      console.log(
        `Stored the ${inputs.cache} directories, hope that helps for next time!`
      );
    } else {
      console.log("Didn't save for some reason?");
    }
  },
};
