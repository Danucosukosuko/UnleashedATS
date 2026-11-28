# Contributing to UnleashedATS

UnleashedATS is a custom firmware with an open door. Features that the [official ATS-Mini firmware](https://github.com/esp32-si4732/ats-mini) turned down, or would not take because they serve a smaller group of users, are welcome here.

## Pull requests

* **Rejected upstream?** Open the same pull request here and link the original one. Say briefly why it was declined, so the discussion does not start from zero.
* A pull request is accepted when it:
  * builds for the OSPI, QSPI and LilyGo variants (the CI checks this),
  * does not break the receiver for people who do not use the feature,
  * can be switched off from the menus when it changes the default behavior,
  * updates the manual in `docs/source/` and adds a fragment in `changelog/` for user-visible changes.
* Fixes that also apply to the official firmware are better sent upstream first, so everyone gets them; they reach UnleashedATS with the next rebase.

## Issues and discussions

* [Issues](https://github.com/Danucosukosuko/UnleashedATS/issues) are for bugs in UnleashedATS. Please say which release (`Settings → About`) and which variant (OSPI or QSPI) you run.
* [Discussions](https://github.com/Danucosukosuko/UnleashedATS/discussions) are for ideas, questions and show and tell.
* Problems that also happen with the official firmware belong [upstream](https://github.com/esp32-si4732/ats-mini/issues).

## Station lists

Station corrections and additions go in `tools/stations/manual/<CC>.json`, see [stations/README.md](https://github.com/Danucosukosuko/UnleashedATS/blob/main/stations/README.md).

## Development

Read [the development guide](https://danucosukosuko.github.io/UnleashedATS/docs/development.html) for the build setup. After editing `ats-mini/web/remote.html`, run `python tools/embed_web.py`.
