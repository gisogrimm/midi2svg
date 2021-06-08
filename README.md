# midi to svg converter for laser-cutting punched tape for barrel organs

## Dependencies

cairomm, midifile (sub-module)

````
sudo apt install libcairomm-1.0-dev
````

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

