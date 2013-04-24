Castor
=======

Constraint-based Search Techniques for RDF

Castor is a protoype [SPARQL][] engine embedding a specialized lightweight
constraint programming (CP) solver. It implements most of the SPARQL 1.0
specification. Currently, queries are performed on read-only datasets created
from RDF graphs in a pre-processing step.

[SPARQL]: http://www.w3.org/TR/2008/REC-rdf-sparql-query-20080115/


Prerequisites
--------------

This project uses the following freely available libraries.

* [Raptor][] for parsing RDF graphs,
* [Rasqal][] for parsing the SPARQL syntax,
* [PCRE][] for handling regular expressions,
* [Mongoose][] (embedded, see `thirdparty/mongoose`) for the HTTP end-point,
* [Google Mock][] and [Google Test][] (embedded, see `thirdparty/googlemock`)
  for unit testing.

[Raptor]: http://librdf.org/raptor/
[Rasqal]: http://librdf.org/rasqal/
[PCRE]: http://www.pcre.org/
[Mongoose]: https://github.com/valenok/mongoose
[Google Mock]: https://code.google.com/p/googlemock/
[Google Test]: https://code.google.com/p/googletest/

You will also need [CMake][] and a C++11 compiler.

[CMake]: http://www.cmake.org/


Building
---------

To build Castor, run

    mkdir build && cd build
    cmake ..
    make

Various build options are available. You can specify them on the command line
of `cmake` or use an interface like `ccmake` to select them.


Running
--------

The following binaries are placed in `build/bin`.

Program  | Description
-------- | ----------------------------------------
castor   | Run a single query
castord  | Simple SPARQL HTTP end-point
castorld | Convert an RDF graph to a Castor dataset
dbinfo   | Introspect a Castor dataset

Run a program without arguments for help about its usage.


Author and license
-------------------

Author: Vianney le Clément de Saint-Marcq <vianney.leclement@uclouvain.be>  
Copyright 2010-2013 Université catholique de Louvain

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 3 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Please refer to the `LICENSE` file in the subfolders of the `thirdparty` folder
for licensing information of third-party software used by this project.
