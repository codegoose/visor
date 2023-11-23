# visor

User interface for simulation racing hardware.

This software is meant to communicate with an Arduino microcontroller over USB and setup a ViGemBus driver-emulated gamepad so it can act as a proxy between steering wheel, pedal inputs and receiving programs. This allows the user to configure input slopes, deadzones, and other settings to fine-tune the inputs.

This fork of the [original repo](https://github.com/SimCoaches/visor) serves to document my solo work on the project. Though the source code for the so-called "MK4", "MK9" Arduino firmware that I also wrote is not publicly available.

The firmware was capable of registering itself over USB as a XYZ-axis & RAW communication device so that it could simultaneously send input axes to the computer, as well as receive input configuration commands from the user interface service. Received information was stored in the EEPROM and used to relay modified pedal inputs in real-time.

I create the user interface by leveraging an OpenGL 3.3 graphics context and [immediate-mode GUI](https://github.com/ocornut/imgui) library that outputs vertex data that's rendered on the GPU. I hide the original hardware input device by interfacing with the [HIDHide](https://github.com/nefarius/HidHide) driver, then spawn a proxy gamepad device to represent the original via the [ViGemBus](https://github.com/nefarius/ViGEmBus) driver.

The software also has the ability to read iRacing telemetry in real-time and display suspension and sway on the screen, among other things. I also did some tests related to automatically braking for the user if they were approaching a corner too fast. And it worked better than you'd think.

### Objects of Interest

* [OpenGL context, window initialization](https://github.com/codegoose/visor/blob/main/libs/boot/imgui_gl3_glfw3.hpp)
* [Arduino USB interface](https://github.com/codegoose/visor/blob/main/libs/firmware/mk4.h) ([implementation](https://github.com/codegoose/visor/blob/main/libs/firmware/mk4.cxx))
* [HIDHide driver interface](https://github.com/codegoose/visor/blob/main/libs/hidhide/hidhide.cxx)
* [iRacing telemetry processor](https://github.com/codegoose/visor/blob/main/libs/iracing/iracing.cxx)
* [Serial port enumeration and IO](https://github.com/codegoose/visor/blob/main/libs/serial/serial.cxx)
* [OpenGL textures, Lottie animations](https://github.com/codegoose/visor/blob/main/libs/texture/texture.cxx)
* [ImGui/FontAwesome5 integration](https://github.com/codegoose/visor/blob/main/libs/font/imgui.cxx)
* [ImGui/OpenGL user interface](https://github.com/codegoose/visor/blob/main/apps/visor/gui.cxx)

## Screenshot

![Visor Screenshot](https://raw.githubusercontent.com/codegoose/visor/main/screenshot0.png)

## Old Demo Video

[![Visor Screenshot](https://img.youtube.com/vi/kaRXmPyVAHs/maxresdefault.jpg)](https://www.youtube.com/watch?v=kaRXmPyVAHs)
