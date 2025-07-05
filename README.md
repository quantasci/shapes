<img src="https://github.com/quantasci/shapes/blob/main/gallery/shapes_teaser.jpg">
<img src="https://github.com/quantasci/shapes/blob/main/gallery/shapes_logo.jpg" width=80%>

# shapes
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
Shapes was designed to be very lightweight and minimal. **The default configuration only requires OpenGL ES3**. 
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

Install by cloning and building Libmin, then cloning and building Shapes.
Steps to Install:
> mkdir codes<br>
> git clone https://github.com/quantasci/libmin<br>
> cd libmin<br>
> ./build.sh<br>
> cd ..<br>
> git clone https://github.com/quantasci/shapes<br>
> cd shapes<br>
> ./build.sh<br>












