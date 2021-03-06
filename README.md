# Aether3D Game Engine

[![Build Status](https://travis-ci.org/bioglaze/aether3d.svg?branch=master)](https://travis-ci.org/bioglaze/aether3d)

This codebase will evolve into the next generation [Aether3D](http://twiren.kapsi.fi/aether3d.html). More info: http://bioglaze.blogspot.fi/2014/12/planning-aether3d-rewrite-for-2015.html

Not expected to be a valuable tool before 1.0 unless for learning purposes.

![Screenshot](/Engine/Assets/sample.jpg)

# Features

  - Windows, macOS, iOS and Linux support.
  - OpenGL 4.1, Vulkan (WIP), D3D12 (WIP) and Metal renderers.
  - Component-based game object system.
  - VR support. Tested on HTC Vive and Oculus Rift.
  - Sprite rendering, texture atlasing and batching.
  - Bitmap and Signed Distance Field font rendering using BMFont fonts.
  - Variance shadow mapping.
  - Audio support for .wav and .ogg.
  - Hot-reloading of assets.
  - Custom model format with .obj, .fbx and Blender exporter.
  - Virtual file system for .pak files.
  - XBox controller support.
  - Cross-Platform WYSIWYG scene editor.
  - Statically linked into your application.
  - Wireframe rendering mode.

# Planned Features

  - Physically-based shading
  - Clustered Forward lighting
  - Post-processing effects
  - Animation
  - Most of the features in my [previous engine](http://twiren.kapsi.fi/aether3d.html)

# API documentation

[link](http://twiren.kapsi.fi/doc_v0.6/html/)

# Build

  - After building build artifacts can be found in aether3d_build next to aether3d.
  - Grab the [sample asset archive](http://twiren.kapsi.fi/files/aether3d_sample_v0.6.zip) and extract it into aether3d_build/Samples after building.

## Windows

  - Open the project in Engine\VisualStudio_* in VS2015 and build it. For MinGW you can use Engine/Makefile.
  - Build and run Samples\01_OpenWindow.
  - Vulkan users: built-in shader sources are located in Engine\assets. If you modify them, you can build and deploy them by running compile_deploy_vulkan_shaders.cmd. 
  - FBX converter tries to find FBX SDK 2017.0.1 in its default install location (English language localization)
  
### Oculus Rift
  - Build Oculus SDK's `LibOVR` and `LibOVRKernel`'s `Release x64` target in VS2015 and copy these folders into `Engine/ThirdParty`: LibOVR, LibOVRKernel, Logging
  - Open OpenGL renderer's Visual Studio solution and select target ***Oculus Release***
  - Build and run Samples/04_Misc3D target ***Oculus Release*** in VS2015.

## macOS

  - Open the project Engine/Aether3D_OSX or Engine/Aether3D_OSX_Metal in Xcode and build it or run the Makefile.
  - Open the project Samples/01_OpenWindow or Samples/MetalSampleOSX and run it or run the Makefile (`make` builds GL version, `make vulkan` builds Vulkan version).
  - FBX converter tries to find FBX SDK 2017.0.1 in its default install location.

## Linux

  - Install dependencies:

`sudo apt install libopenal-dev libx11-xcb-dev libxcb1-dev libxcb-ewmh-dev libxcb-icccm4-dev libxcb-keysyms1-dev`

  - Either run `make -f Makefile_OpenGL` or `make -f Makefile_Vulkan` in Engine.

## iOS
  - Build Aether3D_iOS in Engine. It creates a framework.
  - Open Samples/MetalSampleiOS and add the framework into the project.

# Running Tests

## Visual Studio

  - Unit test project can be found in Engine\Tests\UnitTests. You'll need to set it to run in x64 and copy OpenAL32.dll into the build folder.

## GCC or Clang

  - You can find Makefiles in Engine/Tests.

# License

The engine is licensed under zlib license.

Third party library licenses are:

  - stb_image.c is in public domain
  - stb_vorbis.c is in public domain
  - glxw is under zlib license
  - OpenAL-soft is under LGPLv2 license

# Roadmap/internal TODO

https://docs.google.com/document/d/1jKuEkIUHiFtF4Ys2-duCNKfV0SrxGgCSN_A2GhWeLDw/edit?usp=sharing
