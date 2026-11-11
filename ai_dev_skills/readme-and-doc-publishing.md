# README and documentation publishing

Verified 2026-11-11 by reading the workflows and templates named below and by comparing the live
page with the repository README. Not verified by running Doxygen locally.

## The README is the Doxygen front page

There is no step that generates `README.md`. It is the source, and the Doxygen HTML is generated
from it:

- Every art-daq repo's `doc/CMakeLists.txt` calls `create_doxygen_documentation()` from
  `artdaq-utilities/Modules/artdaq_doxygen.cmake`.
- That macro configures the shared template `artdaq-utilities/Modules/Doxyfile.in`, which sets
  `USE_MDFILE_AS_MAINPAGE = @CMAKE_CURRENT_SOURCE_DIR@/../README.md`.
- A repo can override the template with its own `doc/Doxyfile.in`; otsdaq does not.

So the GitHub README and the front page of https://art-daq.github.io/otsdaq_doxygen/otsdaq/
are the same text, one rendered by GitHub and one by Doxygen. On 2026-11-11 the live page and
`README.md` on develop matched word for word. The same holds for otsdaq-utilities,
otsdaq-components and otsdaq-demo under the same site.

## How the GitHub Pages site is built and published

1. **Build.** otsdaq's `.github/workflows/develop-cpp-ci.yml` calls the reusable workflow
   `art-daq/.github/.github/workflows/develop-cpp-ci.yml@stable` with `otsdaq-mode: true`. It
   builds the suite with Spack in the `ghcr.io/art-daq/otsdaq-spack` container, copies every
   `build/<pkg>/doc/html` into one folder, and uploads it as the artifact named `doxygen`.
   It runs nightly at 07:00 UTC, on pull requests to develop, on pushes to branches other than
   develop and stable, and by manual dispatch.
2. **Publish.** `art-daq/art-daq.github.io` workflow `nightly-ci-dashboard.yml` runs every hour.
   It runs `gh run download --repo art-daq/otsdaq -n doxygen`, adds the artdaq docs and the
   coverage reports, and deploys the org GitHub Pages site. `otsdaq_doxygen` is a folder in that
   site, not a repository, so the GitHub API returns 404 for `art-daq/otsdaq_doxygen`.

Consequences:

- Nothing is triggered by releases or tags.
- A README change on develop appears after the next nightly build plus the next hourly publish,
  so within about a day.
- The publish step takes the most recent run that produced the artifact. Pull request and
  branch-push runs produce it too, so the site can briefly show a README from an unmerged
  branch. Unverified: exactly how `gh run download` without a run ID picks the run.
- To preview a README change before merging, download the `doxygen` artifact from the pull
  request's `build-develop` run in the Actions tab and open `otsdaq/index.html`.

## The separate otsdaq.fnal.gov code docs

https://otsdaq.fnal.gov/docs/code/ (and `/docs/dev/`) is an older, manual path:

- `otsdaq-utilities/tools/OnlineDocPushUpdate.sh`, run by hand from an install area with
  `OTS_WEB_USER` and `OTS_WEB_HOST` set, builds Doxygen locally (`export OTS_DOXY=DOIT`, then
  `mz`), injects `otsdaq-utilities/onlineDoc/inject_<repo>.html` and `inject_otsdaq_head.html`
  into each `index.html` with `doxygen_main_editor`, and copies the HTML there with `scp`.
- It uses the same Doxyfile template, so the README is the base of that front page too, plus the
  injected HTML.
- It is only as current as the last manual run. Nothing in CI calls it.

The otsdaq.fnal.gov tutorials are separate again. The "Installing and Starting otsdaq" page
embeds a Google Slides deck, readable as text at
`https://docs.google.com/presentation/d/<id>/export/txt`.

## README markup that works in both GitHub and Doxygen

- **Section anchors.** Do not use the Doxygen header label `# Title {#label}`. GitHub prints
  `{#label}` as literal text and creates no anchor, so `[x](#label)` links go nowhere. Put an
  HTML anchor on its own line, a blank line, then the header:

  ```
  <a id="frontends"></a>

  Front-ends
  ==========
  ```

  Doxygen documents `<A ID="...">` and `<A NAME="...">` as named anchors. GitHub keeps the
  anchor, prefixing it with `user-content-`, and its page script resolves `#frontends` to it.
- **Lowercase anchor IDs.** GitHub lowercases anchor IDs but not link targets, so a link to
  `#coreSupervisors` does not match. Use lowercase for both.
- **Check the GitHub rendering** without pushing by posting the file to GitHub's Markdown API
  and comparing anchors with links:

  ```bash
  python3 -c "import json; print(json.dumps({'text':open('README.md').read(),'mode':'gfm','context':'art-daq/otsdaq'}))" > /tmp/md.json
  curl -s -X POST https://api.github.com/markdown -d @/tmp/md.json > /tmp/readme.html
  ```

- Images need absolute URLs, for example the logo uses
  `https://github.com/art-daq/otsdaq/blob/develop/doc/logo.png?raw=true`, because the Doxygen
  output lives on a different site.

## Install instructions and where they live

The README's "Installing otsdaq" section has one line for each install, both from
`otsdaq-demo/tools` on develop:

- **otsdaq lite**: `ots-quick-lite-start.sh`, about 5 minutes on a 24-core machine (measured
  4 min 38 s on 2026-11-04). Details are in `otsdaq-demo/lite-quick-install-readme.md`.
- **Full otsdaq**: `ots-quick-spack-start_v1.1.sh`, under an hour with CVMFS or an upstream area
  and more than two hours from scratch. Download the versioned file:
  `ots-quick-spack-start.sh` is a git symlink, and `raw.githubusercontent.com` serves the link
  text instead of the script.

Known mismatch, found 2026-11-11: the install slide deck runs `./ots-quick-spack-start.sh
--use-cvmfs`. Version 1.1 of the script has no `--use-cvmfs` option, so it prints "Unknown
option" and its usage, then exits. CVMFS is already the default in 1.1, and `--no-use-cvmfs`
turns it off. The README omits the option.
