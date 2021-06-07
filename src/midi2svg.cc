#include "MidiFile.h"
#include <iostream>
#include <list>
#include <map>

#include <cairomm/cairomm.h>
#include <cairomm/context.h>
#include <cairomm/enums.h>
#include <cairomm/fontface.h>
#include <cairomm/surface.h>

#include "json.hpp"

#define TASCAR_ASSERT(x)                                                       \
  if(!(x))                                                                     \
  throw std::runtime_error(std::string(__FILE__) + ":" +                       \
                           std::to_string(__LINE__) +                          \
                           ": Expression " #x " is false.")

#define DEBUG(x)                                                               \
  std::cerr << __FILE__ << ":" << __LINE__ << ": " << #x << "=" << x           \
            << std::endl

std::string to_string(double x)
{
  char ctmp[1024];
  sprintf(ctmp, "%g", x);
  return ctmp;
}

std::string get_file_contents(const std::string& fname)
{
  std::ifstream t(fname);
  std::string str((std::istreambuf_iterator<char>(t)),
                  std::istreambuf_iterator<char>());
  return str;
}

// robust json value function with default value:
template <class T>
void parse_js_value(const nlohmann::json& obj, const std::string& key, T& var)
{
  if(obj.is_object())
    var = obj.value(key, var);
}

class note_t {
public:
  int pitch;
  double duration;
  double time;
  void debug();
};

void note_t::debug()
{
  std::cerr << "pitch=" << pitch << " dur=" << duration << " time=" << time
            << std::endl;
}

class midi2svg_t {
public:
  midi2svg_t(const std::string& cfgfile);
  void read(const std::string& midifile);
  void output_svg();
  void generate_svg(const std::string& svgname, double offset_mm);

private:
  bool hasNotes(const smf::MidiEventList& eventlist);
  smf::MidiFile midifile;
  // xercesc::DOMDocument* doc;
  std::map<int, double> pitches;
  double paperwidth;     // mm
  double maxpaperlength; // mm
  double notewidth;      // mm
  double speed;          // mm/s
  double minnotelength;  // mm
  double maxnotelength;  // mm
  double mingaplength;   // mm
  bool cuthighedge;
  bool cutlowedge;
  double offset;        // mm
  double presilence;    // seconds
  double musicduration; // seconds
  std::list<note_t> notes;
  std::string filename;
};

midi2svg_t::midi2svg_t(const std::string& cfgfile)
    : paperwidth(70),      // mm
      maxpaperlength(210), // mm
      notewidth(1.8),      // mm
      speed(8),            // mm/s
      minnotelength(2),    // mm
      maxnotelength(2),    // mm
      mingaplength(6),     // mm
      cuthighedge(false), cutlowedge(false), offset(0.0), presilence(0),
      musicduration(0)
{
  // parse config file
  std::string config(get_file_contents(cfgfile));
  nlohmann::json js_cfg(nlohmann::json::parse(config));
#define PARSEJS(x) parse_js_value(js_cfg, #x, x)
  PARSEJS(paperwidth);
  PARSEJS(maxpaperlength);
  PARSEJS(notewidth);
  PARSEJS(speed);
  PARSEJS(minnotelength);
  PARSEJS(maxnotelength);
  PARSEJS(mingaplength);
  PARSEJS(cuthighedge);
  PARSEJS(cutlowedge);
  PARSEJS(offset);
  PARSEJS(presilence);
  nlohmann::json js_pitches(js_cfg["pitches"]);
  if(js_pitches.is_array()) {
    for(auto pitchrange : js_pitches) {
      int pstart(0);
      int pend(0);
      double pos0(0);
      double deltapos(1);
      parse_js_value(pitchrange, "start", pstart);
      parse_js_value(pitchrange, "end", pend);
      parse_js_value(pitchrange, "p0", pos0);
      parse_js_value(pitchrange, "dp", deltapos);
      if(pstart != 0) {
        for(int pitch = pstart; pitch <= pend; ++pitch) {
          pitches[pitch] = pos0 + (pitch - pstart) * deltapos;
        }
      }
    }
  }
}

void midi2svg_t::output_svg()
{
  double pagestart(0);
  uint32_t page(0);
  while(pagestart < musicduration * speed) {
    char ctmp[1024];
    sprintf(ctmp, "%s_%03d.svg", filename.c_str(), page);
    generate_svg(ctmp, pagestart);
    pagestart += maxpaperlength;
    ++page;
  }
}

void midi2svg_t::read(const std::string& midi_file)
{
  filename = midi_file;
  midifile.read(midi_file);
  midifile.linkNotePairs();  // first link note-ons to note-offs
  midifile.doTimeAnalysis(); // then create ticks to seconds mapping
  for(int k = 0; k < midifile.size(); ++k) {
    smf::MidiEventList& eventlist(midifile[k]);
    if(hasNotes(eventlist)) {
      for(int kevent = 0; kevent < eventlist.size(); ++kevent) {
        auto& event(eventlist[kevent]);
        if(event.isNoteOn()) {
          note_t note({event.getP1(), event.getDurationInSeconds(),
                       event.seconds + presilence});
          if(pitches.find(note.pitch) != pitches.end())
            notes.push_back(note);
          else
            std::cerr << "Warning: note " << note.pitch << " at "
                      << note.time - presilence << " not covered.\n";
          musicduration = std::max(musicduration, note.time + note.duration);
        }
      }
    }
  }
}

void midi2svg_t::generate_svg(const std::string& svgname, double offset_mm)
{
  double scale(72.0 / 25.4001);
  double w(maxpaperlength * scale);
  double h((paperwidth + offset) * scale);
  auto surface(Cairo::SvgSurface::create(svgname, w, h));
  auto cr(Cairo::Context::create(surface));
  cr->scale(scale, scale);
  // cr->translate(0, -offset);
  cr->set_line_width(0.1);
  cr->set_font_size(4);
  cr->set_source_rgb(0, 0, 0);
  // create notes:
  cr->save();
  for(auto note : notes) {
    if(pitches.find(note.pitch) != pitches.end()) {
      double y(pitches[note.pitch]);
      double x(note.time * speed);
      double len(note.duration * speed);
      if(len >= mingaplength)
        len -= mingaplength;
      len = std::min(len, maxnotelength);
      len = std::max(len, minnotelength);
      double x2(x + len);
      if((x2 > offset_mm) || (x < offset_mm + maxpaperlength)) {
        x -= offset_mm;
        x2 -= offset_mm;
        x = std::max(0.0, std::min(maxpaperlength, x));
        x2 = std::max(0.0, std::min(maxpaperlength, x2));
        len = x2 - x;
        if(len > 0) {
          cr->rectangle(x, paperwidth - y - 0.5 * notewidth, std::max(0.0, len),
                        std::max(0.0, notewidth));
          cr->fill();
        }
      }
    }
  }
  cr->restore();
  // cut edges:
  cr->save();
  if(cuthighedge) {
    cr->move_to(0, 0);
    cr->line_to(maxpaperlength, 0);
  }
  if(cutlowedge) {
    cr->move_to(0, paperwidth);
    cr->line_to(maxpaperlength, paperwidth);
  }
  cr->stroke();
  cr->restore();
  // page name:
  cr->save();
  cr->set_source_rgb(1, 0, 0);
  cr->move_to(2, paperwidth - 2);
  cr->text_path(svgname);
  cr->stroke();
  cr->move_to(maxpaperlength, paperwidth);
  cr->line_to(maxpaperlength, paperwidth - 8);
  cr->move_to(maxpaperlength - 20, paperwidth - 4);
  cr->line_to(maxpaperlength, paperwidth - 4);
  cr->move_to(maxpaperlength - 6, paperwidth - 8);
  cr->line_to(maxpaperlength, paperwidth - 4);
  cr->line_to(maxpaperlength - 6, paperwidth);
  cr->stroke();
  cr->restore();
  // crop marks:
  cr->save();
  cr->set_source_rgb(1, 0, 0);
  cr->move_to(0, paperwidth);
  cr->line_to(2.0, paperwidth);
  cr->move_to(0, 0);
  cr->line_to(2.0, 0);
  if(offset > 0) {
    cr->move_to(0, paperwidth + offset);
    cr->line_to(2.0, paperwidth + offset);
  }
  cr->stroke();
  cr->restore();
  cr->show_page();
}

bool midi2svg_t::hasNotes(const smf::MidiEventList& eventlist)
{
  for(int i = 0; i < eventlist.size(); i++) {
    if(eventlist[i].isNoteOn()) {
      if(eventlist[i].getChannel() != 0x09) {
        return true;
      }
    }
  }
  return false;
}

int main(int argc, char** argv)
{
  if(argc < 3) {
    std::cout << "Usage:\n\nmidi2svg <config file> <midi file>\n";
    return 1;
  }
  midi2svg_t m2s(argv[1]);
  m2s.read(argv[2]);
  m2s.output_svg();
  // m2s.generate_svg("page0.svg", 0);
  return 0;
}

/*
 * Local Variables:
 * compile-command: "make -C .."
 * End:
 */
