This directory contains various test files.

Assemble and generate Intel-HEX files by running:

  Windows:        make_test <test-name.asm>
  macOS / Linux:  ./make_test.sh <test-name.asm>

Alternatively, you can simply drag and drop an ASM file onto make_test.bat (Windows).

The macOS / Linux script auto-detects the host OS and uses the matching zmac binary
(zmac.macos or zmac.linux) sitting next to it. Build those binaries from the zmac source
at http://48k.ca/zmac.html and place them in this directory.

Build commands (single C source, no dependencies):

  macOS (universal binary):  cc -O2 -arch arm64 -arch x86_64 zmac.c -o zmac.macos
  macOS (host arch only):    cc -O2 zmac.c -o zmac.macos
  Linux x86_64:              cc -O2 zmac.c -o zmac.linux

Then mark them executable with: chmod +x zmac.macos zmac.linux

Test files:

"hello_world.asm" - default classic 'hello, world' looping test
"test_blank.asm" - prototype test scratch file for simple instruction tests
"test_proto.asm" - prototype test source code, base other tests on it (for more elaborate tests)
"test_daa.asm" - runs DAA instruction on the range of values and then stops
"test_neg.asm" - runs NEG instruction on the range of values and then stops
"test_ints.asm" - test all interrupt modes (IM0/1/2)
"zexall.asm" - classic cpu diagnostic program with the tests sorted by their duration

--------------------
zmac assembler: http://48k.ca/zmac.html
