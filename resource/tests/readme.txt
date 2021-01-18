This directory contains various test files.

Assemble and generate Intel-HEX files by running:
make_test <test-name.asm>

Alternatively, you can simply drag and drop an ASM file onto make_test.bat

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
