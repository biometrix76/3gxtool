# 3gxtool
A utility to create *3GX* plugins from *.elf* and *.plginfo* files.

Note: this version was edited and compiled on OS X to add support, may not work with Windows and Linux. Use the 3gxtool from the original [repo](https://gitlab.com/thepixellizeross/3gxtool/-/releases)

## Building
You need *cmake* in order to build this tool. If you are building on Windows, you need the GNU C++ compiler (for example the one provided by MSYS or MINGW).

### OS X
Make sure to have dynalo and yaml-cpp up-to-date in `extern/dynalo` and `extern/yaml-cpp`, with *XCode* and *cmake* tools installed on your Mac.
```
make
```

## License
Copyright 2017-2022 The Pixellizer Group

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
