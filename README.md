# midi to svg converter for laser-cutting punched tape for barrel organs

Dependencies: libxerces-c, midifile (sub-module)

## Building

````
git clone https://github.com/gisogrimm/midi2svg.git
git submodule update --init
make
````

## Creating an SVG file

````
cd examples
../bin/midi2svg 30note_music_box.js example.midi
````

