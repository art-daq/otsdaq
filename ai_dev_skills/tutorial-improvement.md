# Improving an otsdaq tutorial

How to take one tutorial from the converted Google Slides text to a checked, current page, with new
screenshots. First done for "Installing and Starting otsdaq" on 2026-11-11, on correlator5
(AlmaLinux 9.6, 24 cores) while a live otsdaq lite system was running on the same host.

Related: [readme-and-doc-publishing.md](readme-and-doc-publishing.md) for where the tutorials live
and how they are published; the tutorial site's own guide is
`otsdaq-web/htdocs/sandbox_nov2026/AGENTS.md`.

## Workflow

1. **Read the page:** `otsdaq-web/htdocs/sandbox_nov2026/md/<slug>.md`. Open the original deck
   (link in the comment at the top) when the converted text is unclear.
2. **Check every claim against the source,** not against the slide: script options in
   `otsdaq-demo/tools/*.sh`, messages in `otsdaq/tools/ots`, `UpdateOTS.sh` options in
   `otsdaq-utilities/tools/UpdateOTS.sh`, GUI text in `otsdaq-utilities/WebGUI`. Search the exact
   message text to find where it is printed.
3. **Walk through the steps for real** in an isolated scratch install (next section), recording the
   console output and the time each step takes.
4. **Take screenshots** of what a user sees, with `ai_dev_scripts/ff_screenshot.py`. Screenshot
   only the GUI. Terminal output goes in the page as text in code blocks, trimmed to the lines that
   matter, with host names and paths replaced by `yourhost` and similar placeholders.
5. **Rewrite** the md, following the writing rules below. Name new images by content, e.g.
   `wiz-mode-desktop.png`, and delete images the page no longer uses (git keeps them).
6. **Build and look:** `python3 tools/build_site.py htdocs/sandbox_nov2026` in otsdaq-web, then
   screenshot a few slides of the built page (`file://.../<slug>.html#N`) to check they fit.
7. **Commit, deploy to the sandbox** (rsync command in the site's AGENTS.md), and update the
   status list there.

## Walking through without disturbing a live system

The user's otsdaq may be running on the same host, and an ops agent may be driving it. Never touch
it. `ai_dev_scripts/tutorial_walkthrough_lite.sh <new scratch dir> [--branch B]` does all of this:

- **Scratch area:** a new directory outside every install and source area, never `ots-lite-test`
  or `dev-srcs`.
- **Clean environment:** run each step under `env -i HOME=... USER=... PATH=/usr/bin:/bin ...`
  with `bash --noprofile --norc`, as a new user's terminal would be. This also catches steps that
  only work because of something in your own shell.
- **Own port:** `OTS_WIZ_MODE_MAIN_PORT=<free port>`. The Wiz mode message forwarder then uses
  30000 + port (e.g. 33115), away from a live system's 30000 and 30001. Check
  `ss -ltn`, `ss -lun` before and after.
- **Stopping:** `ots -k` (or `kx`) only kills processes whose command line contains this install's
  `$USER_DATA/` path. It asks "Are you sure ... [Y/N]" and aborts without an answer, so pipe
  `echo Y |`.
- **Proof:** record the PIDs of every other `xdaq.exe`, `ots_lite_main` and emulator process before,
  and compare after.
- **Do not run `reset_ots_tutorial.sh` while any other otsdaq runs on the host.** It begins with
  `ots --killall` (`killall -9 xdaq.exe` for every process of the user, all install areas) and
  `killall -9 ots_udp_hw_emulator`, so it stops every other otsdaq and emulator of that user.
  The installers already install the first_demo data, so after a fresh install there is nothing
  to reset: start it with `ots --wiz` (activates the groups), then `ots`, then
  `ots_udp_hw_emulator 4000 &`. Found 2026-11-12.
- `reset_ots_tutorial.sh` downloads `get_tutorial_data.sh` from GitHub, which accepts only
  `ARTDAQ_DATABASE_URI=filesystemdb://...`; in ots maxlite (`textdb://`) it stops with
  "Environment variable ARTDAQ_DATABASE_URI not setup correctly". See
  `../ai_skills/ots-maxlite-architecture.md` for how the maxlite installer installs the data.
- **Wiz mode only** for install-level tutorials: it starts one web application and no front-ends,
  emulators or UDP traffic. Tutorials that need normal mode and front-ends need a scratch
  configuration with its own ports for every context and the emulator; plan that per tutorial.
- **Full otsdaq (Spack)** was not run: it takes hours and writes Spack configuration under `~/.spack`,
  which other installs of the user share. Check its steps against the script instead, and run only
  paths that stop before doing anything (e.g. argument parsing).

## Screenshots of the otsdaq GUI

`firefox --screenshot URL` captures at the page's load event. The otsdaq desktop adds its icons
after that, inside a frame, so that capture shows an empty desktop. Use
`ai_dev_scripts/ff_screenshot.py`, which drives headless Firefox over its built-in WebDriver BiDi
protocol (Python standard library only, no Selenium):

```bash
# Wiz mode desktop with the first-visit tooltip closed
ai_dev_scripts/ff_screenshot.py "$URL" desktop.png --wait-for ".DesktopIcons-iconDiv" \
    --sleep 3 --eval "Debug.closeErrorPop()"
# a built tutorial slide
ai_dev_scripts/ff_screenshot.py "file://$PWD/htdocs/sandbox_nov2026/<slug>.html#4" s4.png \
    --size 1400x900 --wait-for ".slide.active"
```

- The desktop's content is in a frame. `--wait-for` searches all frames, and `--eval` runs in the
  frame where it matched, where the desktop's JavaScript (`Debug`, `Desktop`, `DesktopContent`) is
  defined.
- Desktop icons have class `DesktopIcons-iconDiv`.
- The first visit to a desktop shows a tooltip (the Wiz mode one is "Wiz-mode Introduction").
  `Debug.closeErrorPop()` closes it, as its Close Tooltip link does.
- Use 1400x820 for GUI shots, so text stays readable when the slide scales them down.

## Writing rules for tutorial pages

- One `##` section per slide, and keep each to one screen. The viewer shrinks text to fit, so a
  crowded slide is hard to read; split it instead.
- Commands in code blocks, exactly as typed, checked to work. Output in code blocks, trimmed.
- Say which install a step is for, otsdaq lite or full otsdaq, when they differ.
- State measured facts with their basis ("4 min 38 s on a 24-core machine") instead of guesses.
- Put the date and otsdaq version the page was checked against on the title slide.
- Remove advice you cannot verify, and say so in the commit message.
- Link other tutorials as `<slug>.md` and the home page as `index.md`; the build makes them `.html`.
- Never write `user@host` in a page: Cloudflare in front of otsdaq.fnal.gov rewrites anything that
  looks like an email address in HTML into "[email protected]", even inside code blocks. Write
  `ssh -l you yourhost.fnal.gov` instead.

## Facts found while fixing "Installing and Starting otsdaq" (2026-11-11)

- The deck's full install command `./ots-quick-spack-start.sh --use-cvmfs` fails: version 1.1 has no
  such option. It prints "Unknown option" and the usage, and exits with status 0, so wrappers do
  not notice. CVMFS is the default; `--no-use-cvmfs` turns it off.
- `setup_ots.sh` must be sourced. The deck's `chmod +x setup_ots.sh; ./setup_ots.sh` has no
  effect.
- `UpdateOTS.sh --tables` (two hyphens; the deck had an en dash) refreshes table definitions from
  `$USER_DATA/ServiceData/CoreTableInfoNames.dat` without pulling. Both installers already install
  the tutorial data, so it is a safe refresh, not a required step.
- `ots -w` prints the Wiz mode URL a few seconds after returning to the prompt. It is also in
  `<install>/tmp/otsActionsHandler_*`. The default access code is `fa37`, length 4, not random,
  set by `Data/ServiceData/OtsWizardData/sequence.dat`.
- The current Wiz mode desktop has no "Layouts" button (older screenshots show one).
- otsdaq lite installed as a new user in 4 min 38 s with the branch installer. On that date the
  `develop` installer still cloned artdaq-database's deleted `feature/ots-lite` branch, so the
  tutorial's `develop` command only works once the installer fixes are merged to develop.
