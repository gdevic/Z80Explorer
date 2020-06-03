// Z80Explorer loads this init script on a startup

// Load "Hello, World" program into the simulated memory

script.response("Loading 'Hello, world' Z80 program\n");
monitor.loadHex("hello_world.hex");
