# pgl-collada
Collada dae file importer and exporter api using expat xml parser

Currently the API is in development but it can import and export the most basic elements from Collada .dae files.
See pgl_asset_collada.h file for which elements are supported.

It uses expat xml parser. You should install it before compiling;
sudo apt install expat

You can build source code by running command below;
gcc src/main.c src/pgl_asset_collada.c -o main.o -lexpat

After compiling main.o, you can run it with test.dae file which shipped with src folder
test.dae file is exported from Blender. It is standart cube model with camera
./main.o "test.dae"

It will print some information about processes to terminal and export a file named "exported.dae"
When exporting the API ignores unsupported elements and it does not export those element tags into file.

test.dae file is exported from Blender. Its sta

