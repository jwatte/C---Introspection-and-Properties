
The "C++ Introspection" library by Jon Watte helps C++ programs introspect data structures for fun and profit!

To build the library and example programs:

Windows, using Visual Studio 2010:
Open the .sln file, and build. This should generate two executables into the solution output directory.

Linux, using GNU make and GCC (any modern version should work):
Type "make" in the directory that contains the Makefile. This should generate two executables into the "bld" directory.

Run the "introspection" executable to verify that the API works as intended.
Run the "simplechat" executable in one of three modes to edit the list of recognized users, run as a server waiting for clients to connect, or run as a client connecting to a server.

To edit the list of recognized user names, run:
simplechat edit

To start a server on a port 4523, run:
simplechat server 4523

To start a client talking to that server, run:
simplechat client MyUserName the.server.name.com 4523

Some notes about this code:
- This code is released under a BSD style open source license. This means that you can use it in programs of your own, on your own risk, by following a very simple copyright notice requirement. See the "LICENSE.txt" file.
- The introspection code is thought to be well tested, and could serve as the basis for a production-worthy system. However, it does not contain all features you will likely need for a real game, network and editor system, so you will have to extend it.
- The sample chat application and server is a sample. It has know bugs: for example, too long input lines from the user will likely crash the program. This is not intended to show how to write secure, robust networking code; rather, it is intended to remove the minutiae of production-ready networking code (which would add complexity), and instead show how the introspection system can be integrated.
- This code is now also described in a chapter in the book Game Engine Gems 2, released at GDC 2011. Please post comments, suggestions and topics for discussion at the support forums for the Game Engines 2 article by me that describes the code and reasoning behind it in more detail:

http://www.enchantedage.com/geg2-introspection

