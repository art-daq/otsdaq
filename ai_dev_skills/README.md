# otsdaq ai_dev_skills index

Knowledge for AI agents (and people) who maintain the otsdaq project itself: documentation,
CI, publishing, and release mechanics. For using and operating otsdaq (configuration, state
machine, emulators, the lite build), see `../ai_skills/README.md` instead.

Each file says what was verified and when, and what is still unverified.

| Skill | Use it to |
|-------|-----------|
| [readme-and-doc-publishing.md](readme-and-doc-publishing.md) | change the otsdaq README or Doxygen front page; how the README becomes the Doxygen site at art-daq.github.io/otsdaq_doxygen (CI, hourly publish, no release step); the separate manual otsdaq.fnal.gov doc push; the otsdaq.fnal.gov website repo (`otsdaq-web`) and tutorial pipeline, including the Markdown tutorial overhaul; README markup that renders in both GitHub and Doxygen; where the install instructions live |

| [tutorial-improvement.md](tutorial-improvement.md) | improve a tutorial page: check each step against the source, walk through it in an isolated scratch install without disturbing a live system, take GUI screenshots (desktop in a frame, first-visit tooltip), writing rules, facts found on the install tutorial |

Scripts for these skills are indexed in `../ai_dev_scripts/README.md`.

## Keeping this index current

Add a row when you add a skill and update the row when a skill's scope changes. Skills should
state what was verified and when, and link to related skills instead of repeating them.
