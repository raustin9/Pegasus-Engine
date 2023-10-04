# Pegasus Game Engine

Welcome to the Pegasus Game Engine!
This is a game engine that I am building to learn how to make platform specific applications along with making a graphics application.

Some helpful resources I am using to help build this:
- P.A. Minerva Tutorial
- Kohi Game engine series on youtube
- Brendan Galea Vulkan Game Engine series

### Overview
In previous engines that I have made, I have used GLFW in order to handle windowing and input. This is a cross platform API, but to better learn how this is handled on each platform, I am not using that in this engine. I will be using an X server for windowing on linux, and the native Windows API for windowing on Windows. 

I will be using the Vukan graphics API to handle rendering as it does not abstract much from the GPU architecture, and this allows for some nice customization and serves as a very good learning experience to **really** understand how the engine works.

### Architecture
###### Platform Interface
Because we are supporting both Windows and Linux OS's, there needs to be a different implimentation of operating system specific functionality.
This can be easily achieved through using a *Platform Interface*. This interface abstracts operating system specific functionality away from the application meaning that we can
write the application as close to OS-agnostic as possible.
This also sets us up well to support other platforms in the future as well like Android, Mac, and even Consoles.

**Important Note**: There are a lot of things that we may not immediately think need to be inside the platform interface layer that actually do. The easiest example of this is windowing. We may think that because we can simply create a windowing API in our platform interface that the application can use, but we don't want to do that. This is because not **ALL** platorms have windowing. If we wrote our application with windowing done above the platform layer, if we wanted to support something like Xbox or iOS, we would have to either write some checks into the application (which goes against the point) or refactor our platform interface API along with the application. This means that we currently handle windowing **inside** the platform layer rather than creating an API for it that will be used by the application.
