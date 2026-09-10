# AtomSlicer

**AtomSlicer Web** is an Emscripten port of [AtomSlicer](https://github.com/iota97/AtomSlicer), bringing the project to the web.

Try the web version here: https://slicer.iota97.workers.dev/.

## Building

The web version is built with **Emscripten 6.0.8**.

```
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
```

## Local Development

You can also run the web server locally.

First, build the web version using Emscripten, then go to the server folder and install the dependencies:

```
cd server
npm install
```

The server uses Express. Start it with:

```
node server.js
```

The web application will be available at:
http://localhost:8080/.

## Citation

If you use this code, please cite:

```
@article{Cocco2026AtomSlicer,
  author = {Cocco, Giovanni and Belle, Vincent and Garner, Eric and Lefebvre, Sylvain and Chermain, Xavier},
  title = {AtomSlicer: Constant-Thickness Field-Aligned Non-Planar Slicing and Continuous Toolpaths for FFF},
  year = {2026},
  doi = {10.1145/3811363},
  journal = {ACM Transactions on Graphics (Proceedings of SIGGRAPH)},
  volume = {45},
  number = {4},
  articleno = {58},
}
```

## Disclaimer

Although the software is designed to avoid collisions, collision-free operation cannot be guaranteed for every setup and printer configuration. Incorrect toolpaths or unforeseen interactions may result in collisions between the printer carriage and the print, potentially damaging your printer. We take no responsibility for any damage, hardware failure, or print failure caused by the use of this software. Users should closely supervise the printer at all times while printing.

## License

MIT License. See [LICENSE](LICENSE) for details.
