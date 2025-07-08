<img src="https://github.com/quantasci/shapes/blob/main/gallery/shapes_teaser.jpg">
<img src="https://github.com/quantasci/shapes/blob/main/gallery/shapes_logo.jpg" width=80%>

# shapes
2020-2025 (c) Quanta Sciences, Rama Hoetzlein<br>

Shapes is a lightweight, node-based renderer for dynamic and natural systems. The goal of shapes is to enable rapid experimentation with dynamic, 
coupled systems, especially those in which the scene graph can change each frame. Shapes makes use of a graph model to evaluate  dynamic _behaviors_ 
that update geometric instances call _shapes_. Shapes incorporates both an OpenGL core rasterizer and OptiX pure raytracing pipelines which can be
switched at run-time. Shapes allows for composition and coupling of complex models through node-graphs. 

## Scene Representation

The scene representation of Shapes consists of a graph of _behaviors_, primitive objects maintained in a list of _assets_, and one or more renderers. 
The output of a behavior is typically a ShapeSet which contains a set of _shapes_. Behaviors, assets and shapes may be the input to any other behavior node.
Assets consist of essential primitives such as meshs, textures, materials, shaders, cameras and globals. Shapes represent an _instance_ of a mesh/geometry and 
refers to a mesh, a shader, a material, several textures and a transform. Together a single shape expresses the render properties, visual appearance and transform 
of a visible object. Shapes are designed to be very lightweight so that they can be animated and add/removed dynamically by behaviors. 

## Features
Features of the Shapes rendering engine include:
* Dynamic scene graphs for motion graphics
* OpenGL ES3 rasterizer, for Windows, Linux or Android
* OptiX pure raytracing 
* Particles systems
* GPU-accelerated SPH Fluids
* Environment lighting
* Cascade Shadow Maps /w PCF Soft shadows
* Projective Displacment Mapping for raytracing
* State sorting for order-independent behaviors
* Trees and Forests (pro version)
* Geospatial Data, LiDAR and DEM (pro version)

## Platforms & System Requirements
Shapes was designed to be very lightweight and minimal. **The default configuration only requires OpenGL and Libmin library**. 
Build flags enable support for OptiX 6.5 for pure raytracing, and NVIDIA CUDA Toolkit for particle system acceleration.

Platforms:
* Windows 10/11 x64
* Linux
* Android

Pre-requisites:
* OpenGL ES3
* Libmin Library
* OptiX 6.5 [optional]
* NVIDIA CUDA Toolkit 10.0+ [optional]

## Installation

Install by cloning and building Libmin, then cloning and building Shapes.<br>
Steps to Install:
```
> mkdir codes
> git clone https://github.com/quantasci/libmin
> cd libmin
> ./build.sh
> cd ..
> git clone https://github.com/quantasci/shapes
> cd shapes
> ./build.sh
>
> ./run.sh
```
The last command will run the scene_basic.txt example in a window.<br>

Typical installation expects that the libmin and shapes repos are sibling folders. A separate folder for /build will be created so that 
the repository trees can remain clean for code modifications. This is the recommended folder structure. 
```
/codes
  |___ /libmin        Libmin repository
  |___ /shapes        Shapes repository
  |___ /build         
        |___/libmin   Libmin build products and libmin.so library
        |___/shapes   Shapes build products and binary
```

## Running 
Several simple samples are provided in the /shapes/assets folder:
1. scene_simple.txt - Simple cube & sphere with environment lighting<br>
2. scene_bump.txt - Bump mapping, with various material properties<br>
3. scene_displace.txt - Displacement mapping for real surface details (CPU demo).<br>
4. scene_scatter.txt - Procedural scattering of one mesh across another.<br>
5. scene_psys.txt - Particle system /w shadows (CPU demo). Press space bar to play.<br>
To run, use:<br>
```
> shapes {scene_file}
```
<br>
Shapes uses the following mouse/keyboard input when run interactively:<br>
* Alt+Left Mouse - Orbit camera
* Alt+Shift+Left Mouse - Pan camera
* Middle Mouse - Pan camera
* Middle Mouse Wheel - Zoom in/out
* Right Mouse - Flying navigation, Change heading
* WSADZQ keys - Flying navigation, W=fwd, S=back, A/D=strafe, Z/Q=raise lower
* Shift+Left Mouse - Move primary light
* '/' - Record button. Save single or multiple frames (if animating)
* 'r' - Reset scene
* ' ' (spacebar) - Play button. Pause/run animation
* Ctrl+S - Save scene
The mouse inputs are designed to match Blender in 3-button mode.

## Background

Shapes is based on an earlier renderer, Luna (2010), which was part of my Ph.D. dissertation on real-time procedural modeling 
in Media Arts & Technology (UC Santa Barbara). That early renderer allowed for each behavior node to output a full hierarchical scene graph; 
however, this introduced complexity and significant overhead. Around 2020, the Shapes Engine was redesigned around the notion of a _shape_, 
a lightweight, transformed instance, which could be efficiently generated, animated, state sorted and culled. This was designed to match
the command-based architecture of zero overhead engines (such as Vulkan, Metal, DX12) although as of 2025 only OpenGL ES3 is implemented. 















