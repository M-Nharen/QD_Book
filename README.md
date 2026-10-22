# HFT QD Master Guide — source

Build:  python3 build.py            (compiles + runs all code, then typesets the PDF)
        python3 build.py --rerun    (force re-running all programs, e.g. for fresh benchmark numbers)
        python3 build.py --sanitize (compile+run every listing under ASan/UBSan; no PDF)

Layout: chapters/*.md   chapter text (small Markdown dialect, see below)
        solutions/*.md  Appendix A solutions per chapter
        code/chNN/*.cpp every listing; first lines may contain  // book: flags=-pthread; norun; timeout=60
        build/          compiled binaries + captured outputs (cache)

Markup: #! Part | subtitle   part page;  # chapter;  ## section (## *title = unnumbered);
        ### subsection;  #### run-in heading;  ```cpp fenced code;  @code chNN/x.cpp listing;
        @output chNN/x.cpp note  captured output;  > note box;  | tables |;  - bullets;
        inline: `code` **bold** *italic* ^{sup} _{sub} [text](url);  @pagebreak
