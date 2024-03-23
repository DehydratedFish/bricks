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
BuildDir: "build"; // Setting the global directory where all built files will be placed

Executable: app {                       // As the name implies an executable is declared.
    build_dir: "build";                 // Changing the build dir only for this executable.
    build_dir #"debug": "build/debug";  // With a # directly following a property it will only be processed
                                        // with the matching build type (see Building).

    include_dirs: "include";            // Adding directories for the compiler to look for header files.
    symbols: "USE_CUSTOM_ALLOCATOR";    // Adding #define symbols.

    sources: "source/main.cpp";         // Adding sources to be build.
    sources: /"source", "memory.cpp";   // A slash before a string adds a sub directory to all following files.
    sources: @win32, "win32_stuff.cpp"; // An @ specifies an available platform identifier and only adds files on this platform.
                                        // / and @ can be used multiple times in one line or split like above.

    dependencies: @win32, "User32.lib", "Ole32.lib", "Dbghelp.lib"; // Needed libraries required for linking.
}
```

Running Bricks will result in an executable `build/app` being compiled from the source file `main.cpp`. A custom #define is added and it looks for header files in a sub folder.

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

## Includes

If you run Bricks with the `register` command and you are in a folder containing a `blueprint` the folder will be added to the include list with it's name.  
ATTENTION: I am lazy and just create a junktion to the folder inside the config of Bricks. Somehow this is only possible with admin privileges on windows.

So if you have a `blueprint` in the folder `utiliy` and run `bricks register` you can now include it in other blueprints.

```
// utility/blueprint
Brick: vector_math {
    sources: "trigonometry.cpp";
}

Library: lib {
    ...
}
```
```
// app/blueprint
Blueprint: utility;

Executable: app {
    ...
    dependencies: utility.vector_math, utility.lib;
}
```

## Building

Just navigate to a folder with a `blueprint` and run `bricks`.  
If you want to register a `blueprint` just type `bricks register`.  
For changing a build type just specify it with `bricks --build_type name` the name can be arbitrary but for `debug` debug symbols are enabled.

Another thing of note are build groups. Running `bricks --group test` will only build Executables that have the property `group: "test";` for example.
