CONTROLS:

RENDER MODE & Light:
1 - WireFrame
2 - Fill
3 - Place Point Light at Camera Location

CAMERA:
WASD & Q/E(UP/DOWN): Camera movement
Right-Mouse + Move: Mouse-Look 

Notes:
/////////////// 
//Renderer
///////////////
load_model("Assets/run.mesh", "Assets/run.mats", "Assets/run.anim"); 
- imports all the binary files passed in


//////////////
// FBXExporter
//////////////

// File to Import
Import is called in FBXExporter.cpp, to change the fbx file to load just change the paramter.
ImportFbx("Assets/Run.fbx");

// Functions
FBXFunctions has all the functionality of the exporter.

Output file names in WriteBinaryFile(), are currently run.mesh, run.mat, run.anim.

// Structs and Defines
All in the header files Material.h->Defines.h->Vertex.h->Structs.h->Mesh.h->FbxFunctions.h