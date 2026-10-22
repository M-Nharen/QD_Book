# Kickoff messages for parallel chats

Each chat (inside the Project, whose instructions say to use the GitHub repo): paste one message.

## Wave 1 (start together)

**Chat 1:** Write Chapters 3 and 4 (DSA I and DSA II) per OUTLINE.md. One chapter per turn if needed. Push to the repo.

**Chat 2:** Write Chapters 5 and 6 (C++ Foundations Bridge; Object Model). ch05.md starts with the Part B divider. Push to the repo.

**Chat 3:** Write Chapters 7 and 8 (Memory Management; Modern C++). Push to the repo.

**Chat 4:** Write Chapters 9 and 10 (Compile-Time Programming; STL Internals, including the full open-addressing hash table). Push to the repo.

**Chat 5:** Write Chapters 11 and 12 (Undefined Behaviour; Computer Architecture). ch12.md starts with the Part C divider. Push to the repo.

**Chat 6:** Write Chapters 13 and 14 (OS & Linux; Concurrency). Chapter 14 is the largest; give it its own turn. Run every concurrent listing under ThreadSanitizer too. Push to the repo.

## Wave 2 (after usage resets)

**Chat 7:** Write Chapters 15 and 16 (Networking; Compilers & Profiling). Push to the repo.

**Chat 8:** Write Chapters 17 and 18 (Microstructure; Low-Latency Components). ch17.md starts with the Part D divider. Push to the repo.

**Chat 9:** Write Chapter 19 (Capstone). If ch14 is already in the repo, reuse and cite its SPSC queue (Section 14.5); otherwise implement it self-contained and cite 14.5 anyway. Push to the repo.

**Chat 10:** Write Chapters 20 and 21 (Interview Strategy with the ~60-question bank and 5 mock sets; PPO). ch20.md starts with the Part E divider. Push to the repo.

**Chat 11:** Write Appendices B (Cheat Sheets), C (Toolchain Setup) and D (Reading List) as chapters/app_b.md, app_c.md, app_d.md, each starting "# B. Cheat Sheets" etc. Only cite sources you are sure exist. Push to the repo.

## Final merge chat

> Merge: pull the repo. Read OUTLINE.md and NOTES.md. Run `python3 build.py --rerun` and `python3 build.py --sanitize`; fix any failures. Check every "Section N.M" / "Listing N.K" / "Chapter N" cross-reference against the built headings and fix mismatches; check that NOTES.md promises are met. Render all pages at low resolution and inspect for overflow or layout problems. Update the title page "working draft" line to final. Commit, push, and send the final HFT_QD_Master_Guide.pdf.
