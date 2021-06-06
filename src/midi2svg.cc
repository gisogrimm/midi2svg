#include "MidiFile.h"
#include <iostream>
#include <list>
#include <map>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>
#include <xercesc/framework/MemBufFormatTarget.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLUni.hpp>

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

class xml_init_t {
public:
  xml_init_t() { xercesc::XMLPlatformUtils::Initialize(); };
  ~xml_init_t() { xercesc::XMLPlatformUtils::Terminate(); };
};

static xml_init_t xercesc_init;

std::basic_string<XMLCh> str2wstr(const char* text)
{
  XMLCh* resarr(xercesc::XMLString::transcode(text));
  std::basic_string<XMLCh> result(resarr);
  xercesc::XMLString::release(&resarr);
  return result;
}

std::basic_string<XMLCh> str2wstr(const std::string& text)
{
  XMLCh* resarr(xercesc::XMLString::transcode(text.c_str()));
  std::basic_string<XMLCh> result(resarr);
  xercesc::XMLString::release(&resarr);
  return result;
}

std::string wstr2str(const XMLCh* text)
{
  char* resarr(xercesc::XMLString::transcode(text));
  std::string result(resarr);
  xercesc::XMLString::release(&resarr);
  return result;
}

std::string get_file_contents(const std::string& fname)
{
  std::ifstream t(fname);
  std::string str((std::istreambuf_iterator<char>(t)),
                  std::istreambuf_iterator<char>());
  return str;
}

void node_set_attribute(xercesc::DOMElement* node, const std::string& name,
                        const std::string& value)
{
  TASCAR_ASSERT(node);
  node->setAttribute(str2wstr(name).c_str(), str2wstr(value).c_str());
}

xercesc::DOMElement* node_add_child(xercesc::DOMElement* node,
                                    const std::string& name)
{
  TASCAR_ASSERT(node);
  return dynamic_cast<xercesc::DOMElement*>(node->appendChild(
      node->getOwnerDocument()->createElement(str2wstr(name).c_str())));
}

void node_set_text(xercesc::DOMElement* node, const std::string& text)
{
  node->setTextContent(str2wstr(text).c_str());
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
  void generate_svg_notes();

private:
  bool hasNotes(const smf::MidiEventList& eventlist);
  smf::MidiFile midifile;
  xercesc::DOMDocument* doc;
  std::map<int, double> pitches;
  double paperwidth;     // mm
  double maxpaperlength; // mm
  double notewidth;      // mm
  double speed;          // mm/s
  double minnotelength;  // mm
  double maxnotelength;  // mm
  double mingaplength;   // mm
  double toolwidth;      // mm
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
      toolwidth(0.7)       // mm
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
  PARSEJS(toolwidth);
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
  // prepare svg header
  xercesc::DOMImplementation* impl(
      xercesc::DOMImplementationRegistry::getDOMImplementation(
          str2wstr("XML 1.0").c_str()));
  TASCAR_ASSERT(impl);
  // initialize svg header:
  doc = impl->createDocument(0, str2wstr("svg").c_str(), NULL);
  auto svg_root(doc->getDocumentElement());
  node_set_attribute(svg_root, "version", "1.1");
  node_set_attribute(svg_root, "xmlns", "http://www.w3.org/2000/svg");
  node_set_attribute(svg_root, "xmlns:xlink", "http://www.w3.org/1999/xlink");
  node_set_attribute(svg_root, "viewBox",
                     "0 0 " + to_string(maxpaperlength) + " " +
                         to_string(paperwidth + toolwidth));
  node_set_attribute(svg_root, "width", to_string(maxpaperlength) + "mm");
  node_set_attribute(svg_root, "height",
                     to_string(paperwidth + toolwidth) + "mm");
}

void midi2svg_t::output_svg()
{
  auto serial(doc->getImplementation()->createLSSerializer());
  auto config(serial->getDomConfig());
  config->setParameter(str2wstr("format-pretty-print").c_str(), true);
  xercesc::MemBufFormatTarget target;
  xercesc::DOMLSOutput* pDomLsOutput(
      doc->getImplementation()->createLSOutput());
  pDomLsOutput->setByteStream(&target);
  serial->write(doc, pDomLsOutput);
  std::string retv((char*)target.getRawBuffer());
  delete pDomLsOutput;
  delete serial;
  std::cout << retv << std::endl;
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
          note_t note(
              {event.getP1(), event.getDurationInSeconds(), event.seconds});
          notes.push_back(note);
          // note.debug();
        }
      }
    }
  }
}

void midi2svg_t::generate_svg_notes()
{
  auto svg_root(doc->getDocumentElement());
  for(auto note : notes) {
    if(pitches.find(note.pitch) != pitches.end()) {
      double y(pitches[note.pitch]);
      double x(note.time * speed);
      double len(note.duration * speed);
      if(len >= mingaplength)
        len -= mingaplength;
      len = std::min(len, maxnotelength);
      len = std::max(len, minnotelength);
      auto svg_rect(node_add_child(svg_root, "rect"));
      node_set_attribute(svg_rect, "x", to_string(x));
      node_set_attribute(
          svg_rect, "y",
          to_string(paperwidth - y + 0.5 * toolwidth - 0.5 * notewidth));
      node_set_attribute(svg_rect, "width",
                         to_string(std::max(0.0, len - toolwidth)));
      node_set_attribute(svg_rect, "height",
                         to_string(std::max(0.0, notewidth - toolwidth)));
    }
  }
  auto svg_line1(node_add_child(svg_root, "line"));
  node_set_attribute(svg_line1, "x1", "0");
  node_set_attribute(svg_line1, "y1", to_string(-0.5 * toolwidth));
  node_set_attribute(svg_line1, "x2", to_string(maxpaperlength));
  node_set_attribute(svg_line1, "y2", to_string(-0.5 * toolwidth));
  node_set_attribute(svg_line1, "stroke", "black");
  auto svg_line2(node_add_child(svg_root, "line"));
  node_set_attribute(svg_line2, "x1", "0");
  node_set_attribute(svg_line2, "y1", to_string(paperwidth + 0.5 * toolwidth));
  node_set_attribute(svg_line2, "x2", to_string(maxpaperlength));
  node_set_attribute(svg_line2, "y2", to_string(paperwidth + 0.5 * toolwidth));
  node_set_attribute(svg_line2, "stroke", "black");
  auto svg_name(node_add_child(svg_root, "text"));
  node_set_attribute(svg_name,"x","4");
  node_set_attribute(svg_name,"y",to_string(paperwidth-4));
  node_set_text(svg_name,filename);
  //node_set_attribute(svg_name,"stroke","#700");
  node_set_attribute(svg_name,"fill","#700");
  node_set_attribute(svg_name,"font-size","6");
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
  m2s.generate_svg_notes();
  m2s.output_svg();
  return 0;
}

/*
 * Local Variables:
 * compile-command: "make -C .."
 * End:
 */
