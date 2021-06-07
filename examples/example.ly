\version "2.16.2"

global = { \key f\major \time 4/4  }

\score {
  \transpose c fis' \new ChoirStaff <<
  \new Staff \new Voice = sopran {
    \clef "treble" \global \relative c' {
      
      f4 f8 g a4 f g e f r a a8 bes c4 a bes g a r
      c4 c c d8 c bes4 c8 bes a4 r
      a4 a a bes8 a g4 c f, r
      
    }
  }
  \new Staff \transpose c' c \chordmode{ 
    f1/c c2 f/c f1/c c2 f/c f1 c2 f d:m g:m/d c:7 f/c
  }
  >>
  \layout {}
  \midi {
    \tempo 4 = 72
  }
}
