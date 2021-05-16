# Endless Web

A fork of [Endless Sky](https://github.com/endless-sky) to make the game playable in a browser.

Play at https://play-endless-sky.com

File issues for anything to do with the browser version of the game here.

File issues for anything to do with game content at [Endless Sky](https://github.com/endless-sky); but please reproduce the issue with the game on desktop first.

### Developing

See instructions at the bottom of [readme-developer.txt](readme-developer.txt) for how to build Endless Web.

Branches get rebased without warning.

### Branches in this repository

* endless-web - This should be whatever is live at play-endless-sky.com. This branch contains changes it would never make sense to upstream to Endless Sky. Make pull requests against this branch.
* browser-support - Changes that could upstreamed to Endless Sky. This branch is frequently rebased.
* master - This is the last version of [endless-sky/endless-sky](https://github.com/endless-sky/endless-sky) that the es-wasm branch is rebased on top off.

### Authors

This port of Endless Sky to the web was created by janisozaur and Tom Ballinger. The game wasn't! See [credits.txt](credits.txt) for that.
