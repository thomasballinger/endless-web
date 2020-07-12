"use strict";

(function (exports) {
  function rmdashr(path) {
    const f = FS.open(path);
    const paths = Object.keys(f.node.contents);
    FS.close(f);
    if (f.node.isFolder) {
      for (const childPath of paths) rmdashr(path + "/" + childPath);
      FS.rmdir(path);
    } else {
      FS.unlink(path);
    }
  }

  function showPlugins(container, plugins) {
    container.innerHTML = "";
    container.style.display = "flex";
    container.style.flexDirection = "column";
    container.style.height = "40%";
    container.style.backgroundColor = "grey";
    container.style.overflow = "scroll";
    plugins.forEach(
      ({ name, url, version, author, iconUrl, description }, i) => {
        // TODO figure out a way to show icons (proxy?)
        const row = document.createElement("div");
        row.style.display = "flex";
        row.style.alignItems = "center";
        row.style.flexBasis = 1;
        row.style.flexGrow = 1;
        row.style.flexShrink = 1;
        row.style.padding = "5px";
        row.innerHTML = `
      <input style="flex: 1;" type="checkbox" name="plugin-${i}"/>
      <p style="flex: 2;" class="plugin-title"></p>
      <a style="flex: 1;" class="plugin-link" target="_blank" href="">link</a>
      <p style="flex: 6;" class="plugin-description"></p>
      <p style="flex: 2;" class="plugin-author"></p>
      `;

        // add dynamic values using DOM apis to prevent XSS injection
        row.querySelector("input").value = url;
        row.querySelector(".plugin-title").textContent = name;
        row.querySelector(".plugin-link").href = url;
        row.querySelector(".plugin-description").textContent = description;
        row.querySelector(".plugin-author").textContent = author;

        const proxy = "https://cors-anywhere.herokuapp.com/";
        const githubZip = `${proxy}${url}/archive/master.zip`;
        const pluginPath = url.split("/").reverse()[0] + "-master";
        container.appendChild(row);

        row
          .querySelector("input")
          .addEventListener("change", async function (e) {
            if (this.checked) {
              this.disabled = true; // disable until download complete

              const data = await new Promise(function (resolve, reject) {
                JSZipUtils.getBinaryContent(githubZip, function (err, data) {
                  if (err) reject(err);
                  else resolve(data);
                });
              });
              const jszip = await JSZip.loadAsync(data);
              for (const name of Object.keys(jszip.files)) {
                const zipObj = jszip.files[name];
                if (zipObj.dir) {
                  const path = ("/plugins/" + name).slice(0, -1); // remove trailing slash
                  FS.mkdir(path);
                  continue;
                } else {
                  const ab = await zipObj.async("ArrayBuffer");
                  const stream = FS.open("/plugins/" + name, "w+");
                  FS.write(stream, new Uint8Array(ab), 0, ab.byteLength, 0);
                  FS.close(stream);
                }
              }
              this.disabled = false;
            } else {
              const path = `/plugins/${pluginPath}`;
              rmdashr(path);
            }
          });
      }
    );
  }

  window.showPlugins = showPlugins;
})();
