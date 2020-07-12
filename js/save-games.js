"use strict";

(function (exports) {
  function timeDelta(seconds) {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor(seconds / 60);
    const s = Math.ceil(seconds % 60);
    if (h) return h > 1 ? `${h} hours ago` : `1 hour ago`;
    if (m) return m > 1 ? `${m} minutes ago` : `1 minute ago`;
    return s < 20 ? `just now` : `${s} seconds ago`;
  }

  function showSaveGames(container) {
    const now = new Date();
    const contents = FS.lookupPath("saves").node.contents;
    const files = Object.keys(contents).map((name) => {
      const path = `/saves/${name}`;
      const mtime = FS.stat(path).mtime;
      return {
        name,
        path: `/saves/${name}`,
        mtime,
        t: +mtime,
        secondsAgo: Math.ceil((now - mtime) / 1000),
      };
    });
    files.sort((a, b) => a.secondsAgo - b.secondsAgo);

    container.innerHTML = "";
    files.forEach(({ name, path, secondsAgo }) => {
      const button = document.createElement("button");
      button.class = "download-button";
      button.innerText = `${name} (saved ${timeDelta(secondsAgo)})`;
      button.onclick = function offerFileAsDownload() {
        const mime = "text/plain";
        let content = FS.readFile(path);
        console.log(
          `Offering download of "${path}", with ${content.length} bytes...`
        );

        const a = document.createElement("a");
        a.download = name;
        a.href = URL.createObjectURL(new Blob([content], { type: mime }));
        a.style.display = "none";

        document.body.appendChild(a);
        a.click();
        setTimeout(() => {
          document.body.removeChild(a);
          URL.revokeObjectURL(a.href);
        }, 10000);
      };
      container.appendChild(button);
    });
  }

  window.showSaveGames = showSaveGames;
})();
