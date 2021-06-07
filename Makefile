all: bin/.dir lib bin/midi2svg

CXXFLAGS += -Imidifile/include/
LDLIBS += -lmidifile
LDFLAGS += -Lmidifile/lib/

CXXFLAGS += -Wall

EXTERNALS = cairomm-1.0

LDLIBS += `pkg-config --libs $(EXTERNALS)`
CXXFLAGS += `pkg-config --cflags $(EXTERNALS)`

lib:
	$(MAKE) -C midifile library

bin/%: src/%.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

%/.dir:
	mkdir -p $(dir $@) && touch $@

clean:
	rm -Rf bin
