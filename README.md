# L2TE - Tile Editor

Layered 2D tile editor


## Layers
L2TE is designed to use 7 layers.
The default designation for these layers are as follows
* 0 - background 0
* 1 - background 1 
* 2 - foreground 0
* 3 - foreground 1
* 4 - collision
* 5 - player & npc & object
* 6 - event


## structure
When creating a new map, a folder is automatically created in the asset/map/<name> folder.
This folder contains the metadata file, and each layer as a separate file.

**Example**
<br>
Creating the map "test_map",

 asset/map/test_map/
* test_map_00.mp
* test_map_01.mp
* test_map_02.mp
* test_map_03.mp
* test_map_04.mp
* test_map_05.mp
* test_map_06.mp
* test_map_07.mp
* test_map.md 