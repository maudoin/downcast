Downcast
========

Filter and download podcast to play on a **Portable Media Player**.

Each podcast can be downloaded in it's own folder and filenames are prefixed with the publication date for easy usage in cheap/basic/old **Portable Media Players**.

**:warning: NOTE: This is not meant to play podcasts, just download them to listen offline on the go without draining your phone batteries!!**

Programmed in modern C++.

The executable is portable (no installation or .dll required, see the third parties below). 
A database file is produced along the executable to keep track of the show download / filtering status.

Screen shots
------------
![Main window](/resources/screenshot-main.png)
![Subscription](/resources/screenshot-add-preview.png)

TODO
----

- [x] Windows build
- [x] Fix dummy manifest setup
- [ ] Proper 3rd parties setup instead of file copies
- [ ] Other OS support
- [ ] Unit tests
- [ ] Proper multi platform C.I.
- [ ] A lighter user interface or even a TUI ? (FXTUI, Turbo Vision, PDCurses...)

Third parties
-------------

Compiled with :
- **GCC 12.2.0 + LLVM/Clang/LLD/LLDB 15.0.6 + MinGW-w64 10.0.0 (MSVCRT) - release 2** Win64 from winlibs.com (https://github.com/brechtsanders/winlibs_mingw/releases/download/12.2.0-14.0.6-10.0.0-msvcrt-r2/winlibs-x86_64-posix-seh-gcc-12.2.0-llvm-14.0.6-mingw-w64msvcrt-10.0.0-r2.7z
(see https://winlibs.com/)
- CMAKE 3.19.2 (see https://cmake.org)
- Windows 10 Kit 10.0.19041.0 (https://go.microsoft.com/fwlink/?linkid=2120735)


Source included 3rd parties:

- cpp-httplib : https://github.com/yhirose/cpp-httplib
- RapidXml : https://rapidxml.sourceforge.net/
- SQLite : https://www.sqlite.org/amalgamation.html
- NanoJPEG : https://keyj.emphy.de/nanojpeg/
- picoPNG : https://lodev.org/lodepng/
- Dear ImGui : https://github.com/ocornut/imgui : 
- imguiapp: https://github.com/pplux/imgui-app (based on SOKOL)
- imgui-filebrowser : https://github.com/AirGuanZ/imgui-filebrowser
