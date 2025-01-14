// Z80Explorer loads this init script on a startup

// If you make any changes to this file while the app is running, simply reload it
// by dragging and dropping it into the app's Command Window

function loadHex(fileName) { mon.loadHex(fileName); }
function loadBin(fileName, address) { mon.loadBin(fileName, address); }
function saveBin(fileName, address, size) { mon.saveBin(fileName, address, size); }
function echo(s) { mon.echo(s); }
function readMem(address) { return mon.readMem(address); }
function writeMem(address, value) { mon.writeMem(address, value); }
function readIO(address) { return mon.readIO(address); }
function writeIO(address, value) { mon.writeIO(address, value); }
function stopAt(hc) { mon.stopAt(hc); }
function breakWhen(net, value) { mon.breakWhen(net, value); }
function set(name, value) { mon.set(name, value); }
function setAt(name, hcycle, hold) { mon.setAt(name, hcycle, hold); }
function setAtPC(name, address, hold) { mon.setAtPC(name, address, hold); }
function setLayer(id) { img.setLayer(id); }
function addLayer(id) { img.addLayer(id); }
function setZoom(value) { img.setZoom(value); }
function setPos(x, y) { img.setPos(x, y); }
function find(feature) { img.find(feature); }
function show(x, y, w, h) { img.show(x, y, w, h); }
function state() { img.state(); }
function annot(fileName) { img.annot(fileName); }

// Callback for keys that are not handled by the image view
// You can show the values of (code,ctrl) if you hold down the Alt key while pressing your key combination
function key(code, ctrl)
{
    // Few useful image layer presets
    if (code == 33) // key "1" shifted, '1' on a US keyboard layout
    {
        setLayer("3"); addLayer("4"); addLayer("5"); addLayer("6"); addLayer("7");
    }
    if (code == 64) // key "2" shifted, '@' on a US keyboard layout
    {
        setLayer("3"); addLayer("4"); addLayer("6"); addLayer("7");
    }
    if (code == 35) // key "3" shifted, '#' on a US keyboard layout
    {
        setLayer("a"); addLayer("b"); addLayer("d"); addLayer("e");
    }

    //-------------------------------------------------------------------------------
    // F7 - run 1 half cycle forward
    if ((code == 0x1000036) && (ctrl == 0))
        run(1);
    // F8 - run 2 half cycles forward
    if ((code == 0x1000037) && (ctrl == 0))
        run(2);
    // Ctrl + F7 - run 1 half cycle back (by reset and rerun)
    if ((code == 0x1000036) && (ctrl == 1))
    {
        cycle = mon.getHCycle();
        if (cycle >= 10)
        {
            reset();
            run(cycle - 9);
        }
    }
    // Ctrl + F8 - run 2 half cycles back (by reset and rerun)
    if ((code == 0x1000037) && (ctrl == 1))
    {
        cycle = mon.getHCycle();
        if (cycle >= 11)
        {
            reset();
            run(cycle - 10);
        }
    }
    //-------------------------------------------------------------------------------
}

function help()
{
    print("load(\"file\")   - Loads and executes a JavaScript file (\"script.js\" by default)");
    print("run(hcycles)   - Runs the simulation for the given number of half-clocks or 0 for all");
    print("stop()         - Stops the running simulation");
    print("reset()        - Resets the simulation state");
    print("t(trans)       - Shows a transistor state");
    print("n(net|\"name\")  - Shows a net state by net number or net \"name\"");
    print("eq(net|\"name\") - Computes and shows the logic equation that drives a given net");
    print("print(\"msg\")   - Prints a string message");
    print("relatch()      - Reloads all custom latches from 'latches.ini' file");
    print("save()         - Saves all changes to all custom and config files");
    print("-- Object 'monitor' methods:");
    print("mon.loadHex(\"file\") - Loads a HEX file into simulated memory");
    print("mon.loadBin(\"file\",address) - Merges a binary file into simulated memory at address");
    print("mon.saveBin(\"file\",address,size) - Saves the content of the simulated memory to a file");
    print("mon.echo(ascii) - Echoes ASCII code to the monitor output terminal");
    print("mon.echo(string) - Echoes string message to the monitor output terminal");
    print("mon.readMem(addr) - Reads a byte from the simulated memory");
    print("mon.writeMem(addr,value) - Writes a byte to the simulated memory");
    print("mon.readIO(addr) - Reads a byte from the simulated IO space");
    print("mon.writeIO(addr,value) - Writes a byte to the simulated IO space");
    print("mon.stopAt(hcycle) - Stops the simulation at a given half-cycle number");
    print("mon.breakWhen(net,value) - Stops the simulation when a given net number becomes 0 or 1");
    print("mon.set(\"name\",value) - Sets an output pin (\"int\",...) to a value");
    print("mon.setAt(\"name\",hcycle,hold) - Activates an output pin at hcycle and holds it for hcycles");
    print("mon.setAtPC(\"name\",address,hold) - Activates an output pin when PC equals the address and holds it for hcycles");
    print("mon.enabled = [1|0] - Variable: Enables or disables monitor’s memory mapped services at the address 0xD000");
    print("mon.rom = [size] - Variable: Designates initial length as read-only memory");
    print("--- Object 'image' methods:");
    print("img.setLayer(id) - Sets the layer id ('1'...'k'");
    print("img.addLayer(id) - Adds the layer id ('1'...'k' to the existing image");
    print("img.setZoom(value) - Sets the zoom value (from 0.1 to 10.0)");
    print("img.setPos(x,y) - Moves the image to coordinates; x: 0 – 4700, y: 0 – 5000");
    print("img.find(\"feature\") - Finds and shows the named feature");
    print("img.show(x,y,w,h) - Highlight a rectangle at given coordinates");
    print("img.state() - Prints the current image view position and zoom to the log window");
    print("img.annot(\"file.json\") - Loads a custom annotation file to all image views");
    print("* Specifying 'mon.' or 'img.' prefix is optional when calling functions.");
}

// Load "Hello, World" program into the simulated memory

print("Loading 'Hello, World' Z80 program\n");
loadHex("hello_world.hex");
print("Read the Online Manual, type help() or run(0) to start...");
