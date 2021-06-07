all: bin/.dir lib bin/midi2svg

CXXFLAGS += -Imidifile/include/
LDLIBS += -lmidifile -lxerces-c
LDFLAGS += -Lmidifile/lib/

CXXFLAGS += -Wall

lib:
	$(MAKE) -C midifile library

bin/%: src/%.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

%/.dir:
	mkdir -p $(dir $@) && touch $@

clean:
	rm -Rf bin
