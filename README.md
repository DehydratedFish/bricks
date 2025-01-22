# Overview
Bricks is a very simple build helper currently for windows. I just wanted something to quickly get a project going and be able to compile it all in just one command.
It does no incremental building and just compiles everything from scratch all the time. This means it is probably not suitable for any bigger applications but it is not meant to be.

# Usage
## Installation
Just clone the source code and run the build.bat file. Currently only MSVC is supported for compiling Bricks and building projects.
As soon as you add the bricks.exe to your path you are good to go.

## Blueprints
Blueprints are the files used to describe your stuff. Put a file called `blueprint` inside a folder and then run Bricks to build.
Here is a basic example:

```
// Imports first look for a matching subdirectory and after that in the brickyard.
import "libraries/os" as os;


// Setting the global directory where all build files will be placed.
build_folder: "build";

executable: app {                       // As the name implies an executable is declared.
    folder: "build/debug";              // Changing the build dir only for this executable.
    folder(release): "build/release";   // Fields can specify for which build types they are
                                        // included in paranthesis. The default is debug.

    include: "include";                 // Adding include folders for the compiler.
    symbols: "USE_CUSTOM_ALLOCATOR";    // Adding #define symbols.

    sources: "source/main.cpp";         // Adding sources to be build.
    sources: /"source", "memory.cpp";   // A slash before a string adds a sub directory to all following files.
    sources(#win32): "win32_stuff.cpp"; // Appending a # before the identifier in the parenthesis specifies
                                        // the target platorm the field is included.

    dependencies: os.core;              // Including a brick or library from another module.
    dependencies(#win32): "Dbghelp.lib"; // Needed libraries for the linker.
}
```

Running Bricks will result in an executable `build/debug/app` being compiled from the source file `main.cpp`. A custom #define is added and it looks for header files in a sub folder.

Quick and easy made build file in my eyes. There is some more fuctionality that makes the builds a little more dynamic. As in other build systems there is a way to make libraries(for the moment only static libraries) as well.

```
Library: basic {
    ...
}
```

Now you just add the declared Library as a dependency in an Executable and it will be build before and linked.

```
Executable: app {
    ...
    dependencies: basic;
    ...
}
```

A thing I so far missed from other build systems (or maybe I just wasn't aware of it) is to just add files and compile them directly into a executable or library.
Here comes an additional think into play called a Brick. Which is basically just a collection of properties that can be added to other stuff.

```
Brick: util {
    // Declare includes, symbols, sources, dependencies.
}

Executable: app {
    ...
    dependency: util;
    ...
}
```

Now all declared files and symbols are added to app.

## Brickyard (imports)

If you run Bricks with the `register` command and you are in a folder containing a `blueprint` the folder will be added to the include list with it's name.  

So if you have a `blueprint` in the folder `utiliy` and run `bricks register` it will be registered to the brickyard and can be imported in any blueprint.

## Building

Just navigate to a folder with a `blueprint` and run `bricks`.  
If you want to register a `blueprint` just type `bricks register`.  
For changing a build type just specify it with `bricks --build_type name` the name can be arbitrary but for `debug` debug symbols are enabled.

Another thing of note are build groups. Running `bricks --group test` will only build Executables that have the property `group: "test";` for example.
