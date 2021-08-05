'use strict';

(function(exports) {

  // A simple key value store using IndexedDB because it allows storage of
  // large amounts of data.
  class KeyValueStore {
    constructor(dbName, storeName) {
      this.storeName = storeName
      this.dbPromise = new Promise((resolve, reject) => {
        const dbRequest = indexedDB.open(dbName, 1);
        dbRequest.onerror = () => reject(dbRequest.error);
        dbRequest.onsuccess = () => resolve(dbRequest.result);
        dbRequest.onupgradeneeded = () => {
          dbRequest.result.createObjectStore(storeName);
        }
      });
    }
    get(key) {
      return this.dbPromise.then(db => new Promise((resolve, reject) => {
        const transaction = db.transaction(this.storeName, 'readonly');
        const req = transaction.objectStore(this.storeName).get(key);
        transaction.oncomplete = () => resolve(req.result);
        transaction.onabort = transaction.onerror = () => {
          reject(transaction.error);
        }
      }))
    }
    set(key, value) {
      return this.dbPromise.then(db => new Promise((resolve, reject) => {
        const transaction = db.transaction(this.storeName, 'readwrite');
        const req = transaction.objectStore(this.storeName).put(value, key);
        transaction.oncomplete = () => resolve(req.result);
        transaction.onabort = transaction.onerror = () => {
          reject(transaction.error);
        }
      }))
    }
  }

  const kvstore = new KeyValueStore("data", "store");

  // Caches a single version of a resource
  class CachedResource {
    constructor(resourceUrl) {
      this.resourceUrl = resourceUrl
    }
    async get(version, progressCallback=()=>{}) {
      const cachedData = await kvstore.get(this.resourceUrl);
      const cachedVersion = await kvstore.get(this.resourceUrl + "-version")
      if (cachedData) {
        if (version === cachedVersion) {
          console.log('Using cached resource', this.resourceUrl)
          progressCallback(cachedData.byteLength, cachedData.byteLength, true);
          console.log('cached data:', cachedData)
          return cachedData;
        }
        console.log('Out of date resource, redownloading', this.resourceUrl)
        console.log('required version', version, 'but had version', cachedVersion, 'stored');
        return cachedData;
      }
      let data;

      const response = await fetch(this.resourceUrl)
      const length = parseInt(response.headers.get('Content-Length'));
      if (length) {
        let offset = 0;
        data = new ArrayBuffer(length);
        const view = new Uint8Array(data);
        progressCallback(offset, length)
        const reader = response.body.getReader();
        while (true) {
          const {done, value} = await reader.read();
          if (done) break;
          view.set(value, offset);
          offset += value.length;
          progressCallback(offset, length)
        }
        progressCallback(offset, length)
      } else {
        data = await response.arrayBuffer();
      }

      console.log('downloaded', this.resourceUrl, data);
      try {
        await kvstore.set(this.resourceUrl, data);
        await kvstore.set(this.resourceUrl + "-version", version);
      } catch (e) {
        console.log('Failure writing to IndexedDB, maybe private browsing / incognito mode or low on disk space');
      }
      return data;
    }
  }

  window.CachedResource = CachedResource;
})();
