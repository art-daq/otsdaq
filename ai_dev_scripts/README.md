# otsdaq ai_dev_scripts index

Scripts for agents (and people) who maintain otsdaq itself: documentation and tutorials. The
recipes behind them are in `../ai_dev_skills/README.md`. Run any script with `--help` or without
arguments for its usage.

| Script | Use it to |
|--------|-----------|
| [ff_screenshot.py](ff_screenshot.py) | screenshot a web page in headless Firefox after it has finished drawing: wait for an element (in any frame), run JavaScript such as a click, capture the window or one element. Python standard library only. |
| [tutorial_walkthrough_lite.sh](tutorial_walkthrough_lite.sh) | install otsdaq lite in a new scratch directory as a new user would, start Wiz mode on a free port, take the standard screenshots, stop, and prove no other otsdaq process on the host was touched |
