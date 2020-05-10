This directory contains sources for test files.

Assemble and generate Intel-HEX files by running:
make_test <test-name.asm>

Alternatively, you can simply drag and drop an ASM file onto make_test.bat

Test files:

"hello_world.asm" - test UART print out functions
"test_proto.asm" - prototype test source code, base other tests on it
"test_daa.asm" - runs DAA instruction on the range of values and then stops
"test_neg.asm" - runs NEG instruction on the range of values and then stops
"zexall.asm" - classic cpu test program but with the tests sorted by their duration

--------------------
zmac assembler: http://48k.ca/zmac.html
