# RCP-Host

RCP-Host is a simple host side implementation that wraps RCP packets in easier to use C structs. The primary
functionality is contained in the `RCP_poll` function, which takes in available bytes and calls the appropriate 
callback functions. The RCP specification can be found in `RCP.md`. 

This project is primarily meant to be used in conjunction with 
[RCI](https://github.com/liquid-rocketry-illinois/LRI), but it can also be used as inspiration for other 
implementations if needed.

# RCP

RCP, or the Rocket Control Protocol, is a simple protocol designed to facilitate communication between an apparatus 
and LRI members. It organizes rocket components into "device classes", and identifies a specific device by its class 
and an 8-bit ID number. Using this identifier, both sensors and actuators can be communicated with and controlled. 
The full RCP specification can be read in [RCP.md](./RCP.md).