# QtBuilder
Automated building of Qt libraries for x64/x86, shared/static and different MSVC versions (Windows only)

Heavy WIP !!!

PRECONDITIONS

- Uses imdisk to speed up building; depending on the Qt version and included options 8-12GB RAM is needed!
...imdisk is available here: http://www.ltr-data.se/opencode.html/#ImDisk
- Can be configured to use other disks instead (what of course substantially slows down builindg).
- Needs jom in the /bin folder of the base Qt installation.
...jom is available here: https://download.qt.io/official_releases/jom/

*** ATTENTION ***

This stuff messes with the file system; there are some checks, but misconfigured values and/or bugs may easily delete whole drives, format disks, etc. etc.

In it's current state this is only - and only - useful for me and anybody who exactly understands what the app is doing and why.

_Most important_: When running the app it immideately starts working based on the configured variables; normally it will just quit if anything is missing, but since the GUI has not much to show, apart from logging lines, it is definitely not recoomended to just try it without knowledge about the internals.

IMPORTANT NOTES

- As of now this is strictly tailored to my needs; Configuration needs to be done in code but is more or less easy to get.
- Tailored for use with MSVC; the created targets can be easily used in Qt Creator though (by adding custom kits)
- Currently only tested with Qt 4.8.7 (since i need to stay with Qt4 for my projects)
- No code comments!
- Might not work for you, or you and even you.

HOW & WHAT

1. A previously installed Qt version is cloned to a RAM disk volume (skipping unneeded parts)
2. A target structure is created like: Drive:\...\Type[shared|static}\Platform[Win32\x64]\MsvcToolset[v100|v120|...}
...means the $(PlatformToolset) and $(Platform) vars from MSVC can be used to target the correct libraries (i.e. for .props)
3. Depending on the required builds a matching environment is created for Qt configure and jom
4. Qt "configure", "jom" and "jom clean" are executed; Qt configuration options are set from predefined lists.
5. After a successful build, the result is copied int the newly created target folder (per build type)
6. The next build starts, until everything is completed.

RESULTS

A Qt build without webkit, declarative, accessibility, phonon (and a few more), lasts about 10 mins (minutes) for a shared x86 build and ~5 mins more for a static x64 build, including copying files (source Qt installed on a SSD, targets on 15krpm SAS disks, Quadcore i7)

WHY & WHEN

After having spent a week to build several Qt versions over and over because of problems, missing stuff, linker errors afterwards, etc. etc. i got tired to do this all by hand, sort files and folders, and all that, so i spent a couple of days to create this.

During typing this readme, 8 builds of Qt for VS 2010/13 (shared+static, x64+x86) were successfully and completely automated done, so i consider it worth the work.

It would be of course nice to extend the GUI from plain logging to provide configurability; i.e. to select which modules to include and exclude, which versions to build and a couple things more. Also it might need additional work to be compatible with Qt5. I might address both aspects later, as i probably will also need Qt5 at some point.

For now this is just "AS IS".

CODE STATE

... in testing; Code is about to follow as soon I have built a complete set of Qt libs for VS2010/2013 in all flavors and could compile all my projects with all of them, in VS as well as in Qt Creator.

SUPPORT

None!
