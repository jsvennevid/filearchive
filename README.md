filearchive
===========

Easy to use library for archive-based file creation and access.

License
-------

Copyright (c) 2010 Jesper Svennevid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Compiling
--------

The build system in use is tundra, found at:
https://github.com/deplinenoise/tundra

Features
--------

* The library itself supports creation of archives without supporting tools, allowing for easy embedding within software projects.

* Data can be accessed through its content hash, allowing for alternate ways of access; no name collision tests are done when creating the archive, and names can be empty.

* File format supports embedding itself at the end of another stream of data, allowing for one-file based distribution alongside the software using the contents.

* Optional compression (currently just supporting fastlz, but this can be extended) on a per-file basis.

* Easy to use file-based API.

farc
----

This tool will allow you to carry out operations on file archives. It provides a simple command-based control set that gives you control over what actions are taken on an archive. Currently the tool supports creating, listing entries and viewing content in an archive. The tool only uses operations already existing in the library itself and provides a good example of what can be done.

