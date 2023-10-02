# Pegasus Game Engine

Welcome to the Pegasus Game Engine!
This is a game engine that I am building to learn how to make platform specific applications along with making a graphics application.

#### Overview
In previous engines that I have made, I have used GLFW in order to handle windowing and input. This is a cross platform API, but to better learn how this is handled on each platform, I am not using that in this engine. I will be using an X server for windowing on linux, and the native Windows API for windowing on Windows. 

I will be using the Vukan graphics API to handle rendering as it does not abstract much from the GPU architecture, and this allows for some nice customization and serves as a very good learning experience to **really** understand how the engine works.
