# LRI Rocket Control Protocol (RCP)

## Introduction

This document describes the protocol used with rockets, test stands, or other targets in which low speed status messages
or manual control need to be communicated. This protocol is **not** designed to be used with high speed real time
control or data logging. These functions should be features of the target itself; periodic updates or occasional manual
control may be implemented using RCP.

This protocol does not perform error checking or acknowledgements. Depending on the physical media and lower layers used
to transmit this protocol, these features may already be available.

### Definitions

- Target: The target is the device or apparatus which is being tested, and contains the actuators and sensors in
  question,
  as well as the microcontroller which interfaces between this protocol and the sensors
- Host: This is the computer or device sending control packets. This is likely the computer running the desktop
  software, but it can be any device that sends control packets. The target will only send data back to the host
  when requested, including streaming data.
- Control packet: A packet sent from the host to the target that contains information requesting an update to the
  target state.
- Data packet: A packet sent from the target to the host, containing either the data requested from a control
  packet, or a data point from a stream of data.
- Data streaming: A target state in which data about sensors and actuators is sent to the host continuously to be
  logged or visualized on the host side. This streaming is not meant to be the primary form of data collection. It
  is only so the host can receive periodic updates on the target status.
- Header Byte: The first byte of a packet. It indicates the channel being communicated on, and the length of the rest of
  the packet.
- Class Byte: The second byte of a packet. It indicates the packet "class," or the functionality of the rest of the
  transmitted data.
- Parameter Bytes: The bytes in a packet that follow the class byte.
- Packet length: The length of the packet, not including the header or class bytes.
- ID: A unique identifying number within a device class to identify an individual device. IDs range from 0 - 63
- Device: An actuator, sensor, or other component (including software components) on a target that can be controlled,
  queried, or otherwise interacted with. All devices can be fully qualified with its device class and ID
- Interface: The method by which the host and target communicate with each other. For example, LoRa, USB, TCP/IP over
  various methods, etc.
- Heartbeat packet: Packets sent from the host to the target. If consecutive heartbeats are not received by the
  target in the time specified, the target should initiate an emergency stop.
- Fully Qualified Device Name (FQDN): The 16 bit identifier that makes up a unique number that identifies a device
  on the target. The first 8 bits indicate the device class, the next indicate a device ID. Two devices may share
  the same ID, as long as they are not under the same device class.

### Device Classes

A device class is an 8 bit identifier which encodes the type of device of something. For example, a solenoid, a sensor,
a software component, etc. Device classes, combined with a device ID number (if relevant), form the FQDN and can be
used to uniquely identify an individual device on a target. Some devices, such as the test state, do not require
IDs, since there can only be one such device present.

The defined device classes are as follows:

- 0x00: Test State (virtual device)
- 0x01: Simple Actuator
- 0x02: Stepper Motor
- 0x03: Prompt Input
- 0x04: Angled Actuator
- 0x80: Custom data (virtual device)
- 0x90: Ambient Pressure
- 0x91: Ambient Temperature
- 0x92: Pressure Transducer
- 0x93: Hygrometer (relative humidity)
- 0x94: Load Cell (weight)
- 0x95: Boolean Sensor
- 0xA0: Power Monitor
- 0xB0: Accelerometer
- 0xB1: Gyroscope
- 0xB2: Magnetometer
- 0xC0: GPS

As an aside, devices are loosely assigned class numbers based on a few things. First, all read-only devices have the
MSB set. The next 3 bits for read-only's indicate how many data channels the device has. For example, GPS returns
the latitude, longitude, ground speed, and altitude, thus it has 4 data channels. From there the order is simply
numerical. This is not a strict standard, only a convention that will (hopefully) be maintained.

## Packet Structure

Both data and control packets share the first two bytes.

### Header Byte

The header byte is the first byte of any packet. This contains information on both the channel of communication, and the
subsequent length of data being sent.

#### Channel Bits

The first two bits of the header byte indicate the channel of communication. These 4 channels (00, 01, 10, 11) allow for
multiple target-host setups to be in use at the same time, without interfering with each other. For a target and
host to communicate, each should use and respond to the same channel bits.

Targets and hosts on other channels should not interact with packets from other channels. Each channel is intended
to support only one target and one host, but implementations with multiple targets that do not send data packets are
possible.

#### Length Bits

The remaining 6 bits of the header byte are used to indicate the length of the packet following the class byte.

A length of 0 is reserved for emergency stops. If a target receives a packet with length zero at any time, it should
immediately enter an emergency stop state. This exact state is dependent on the target and it's physical structure, and
when receiving this packet it should complete whatever emergency protocols it has. An emergency stop packet contains
only this header byte, and no other bytes.

Any other length from 1 to 63 specifies the length of the packet excluding this header byte and the class byte.

### Overflows

While it is out of the scope of RCP as this protocol would be an OSI application layer protocol and is not concerned
with transport or guaranteeing delivery, overflows of receiving buffers still have standard behavior. If a buffer 
does overflow, the incomming packet should be discarded.

### Class Byte

This byte indicates the purpose of the rest of the packet, in the form of the device class. The rest of the bytes in
the packet are interpreted based on the device class. This byte is not included in the length field of the header byte.

## Control Packets

Control packets are sent from the host to the target to request an update to the target state, or to request data from
the target. The device to be addressed of a control packet is determined by the value of the class byte, and in some
cases the ID as well.

For every write to a device, the target should send the new state of the device after its state has been changed.
For example, if the host requests a change to a solenoid's state, the target should respond with a solenoid data
packet with the new state of the solenoid. This new state is not necessarily what the host requests, but what, in
reality, is the outcome. This data can be used by the host to determine if the change was successful.

### Read Requests

Any device that can have data read from it will respond to the same packet structure. By sending just the ID of the
device to read from (as well as the class to form the FQDN), the associated device should respond with whatever data
it would normally send back. Some exceptions to this format are noted in device specific sections below.

### The Test State Device

The test state device (device class 0x00) is a special device, in that it is not something physically present, but
rather a software component needed for RCP compliance. Writing to this device controls things like starting, stopping,
and pausing tests, controlling data streaming, and heartbeats. The testing device requires 1 parameter byte (for a
total length of 1). This additional byte specifies which testing function to perform:

- 0x0x: This indicates a request to begin a test. The first 4 bits of this parameter must all be zero, but the
  remaining 4 bits allow for a test number to be selected. This allows for the target to have multiple test sequences
  preprogrammed, and the host can dynamically select which test to start without requiring new code to be uploaded
  to the target.
- 0x10: Full stop the currently running test
- 0x11: Pause/Unpause the currently running test
- 0x12: Request the device performs whatever reset routine it has
- 0x13: Request the device reset its time base for sensor measurements
- 0x20: Stop data streaming
- 0x21: Start data streaming (Full details in data packet section)
- 0x30: Query current test progress (running, paused, stopped, e-stopped, currently streaming data). The target
  should respond with the appropriate data packet.
- 0xF0: Disable heartbeat packets
- 0xFx: Enable heartbeat packets with seconds specified in the second nibble (excluding a value of 15)
- 0xFF: Heartbeat packet
- Remaining codes are undefined behavior

### Simple Actuator Command

This packet class is a manual request to a simple actuator. A simple actuator is any device which is either on or
off. To read from this type of device, the normal read request packet can be used. The response format is indicated
in the data packets section.

To write to a simple actuator, 2 additional parameter bytes are needed. The first is the ID of the actuator to write
to. The next byte indicates the function. The meanings of this parameter byte are indicated below:

- 0x00: Turn actuator off
- 0x80: Turn actuator on
- 0xC0: Toggle actuator state

### Stepper Motor Command

This packet class is a manual request to a stepper motor. To read from this type of device, the normal read request
packet can be used. The response format is indicated in the data packets section.

To write to a stepper motor, additional parameter bytes are required. The first is the ID of the stepper to write to.
The next byte indicates the function. The next 4 bytes are the control value, or in other words the value to set
the stepper motor to. The meanings of the function byte are indicated below:

- 0x40: Absolute positioning
- 0x80: Relative positioning
- 0xC0: Speed based control

The 4 bytes that make up the control value can be interpreted differently depending on the function requested.

### Prompt Inputs

This packet class is used by the target device to prompt input from the host, and like the test state device, is a
software component and has no ID byte. As of now, this prompt request can prompt for either numeric data, or go-no go
authorization, but it can be expanded to include other prompt types. When requesting a prompt, the target device will
send a packet with the following structure:

- Header byte
- Device Class (0x03)
- Prompt Type:
    - 0x00: Go-No Go authorization
    - 0x01: Floating point input
    - 0xFF: Clear Active prompt
- Prompt string (ascii chars). This string DOES NOT CONTAIN a null terminator

The prompt type byte indicates to the host what kind of data the target is expecting to receive back. Only 1 prompt
can be active at a time, and the host should respond to the target with the data type of the latest prompt request.
Depending on the prompt type, the host should respond as follows:

- Go-No Go authorization:
    - Header byte
    - Device Class (0x03)
    - If Go: 0x01
    - If No Go: 0x00
- Floating point input:
    - Header Byte
    - Device Class (0x03)
    - 4 byte floating point number
- Clear Active Prompt: This packet does not contain a prompt string, and is a request from the target for the host
  to discard the last activated prompt. No error should occur if there is no active prompt

The prompt string field of the target side packet contains a string of ascii characters that contain the prompt
string. This prompt string is _not_ null terminated, its length must be determined from the header byte packet
length. This prompt string should be displayed to users when asking for input.

The prompt input device class cannot be queried, and if the host sends a packet to this device without having first
received a prompt request (or an incorrect data type is sent back) then the packet sent by the host should be
ignored by the target.

### Angled Actuator

This packet class is used to control an angled actuator, which refers to any actuators which controls the angle of
something, such as a rocket control surface, a motor shaft, etc. and does not need any other option beyond angle
control. To read from this type of device, the normal read request packet can be used. The response format is indicated
in the data packets section.

To write to an angled actuator, additional parameter bytes are required. The first is the ID of the actuator to
write to. 4 additional bytes form a floating point value to set the actuator to, in degrees.

### Sensor Read Requests

This packet can be used to request a value from a READ ONLY sensor device on the target, or to communicate new
configuration settings for the device. There are multiple versions of this packet:

- Basic sensor request: This packet can be used to only request the value from a sensor device. This packet
  follows the standard read request format.
- Tare request: This packet can be used to tare a sensor. When a tare request is received, the sensor should
  automatically and immediately begin applying this tare value to subsequent data packets. The format is as follows:
    - Header, class, and ID bytes to identify the sensor
    - A data channel byte indicates which data channel (for example, the
      GPS data packet has 4 data channels) to complete the tare on (zero indexed)
    - The tare value, in the sensors native units. For most sensors, this will be a 4 byte floating point value. This
      value is relative to the current data stream, not to the raw data produced by the sensor

---

The remaining class codes are currently undefined behavior.

## Data Packets

These packets are used to communicate state information from the target to the host. Data logging packets begin with
a header byte and a class byte like control packets, and the parameter bytes indicate the data about that device.
The exact format of the data depends on the device.

Data packets also include a 4 byte time stamp immediately following the class byte. This timestamp indicates the
milliseconds since data logging was started. This value **does not indicate absolute time in any way, and is not
intended to be used for accurate timekeeping**. This time value is only for the host to be able to determine the rough
time a data point was produced for data visualization. The timestamp bytes are parameter bytes and thus are included in
the packet length.

### Test State Packet

This packet is used to return the state of a test to the host. This includes 1 additional parameter byte, making the
packet length 5. This additional byte includes several pieces of information:

- The most significant bit indicates if data is currently streaming (1) or not (0)
- The next two bits indicate test state:
    - 00b: Test running
    - 01b: Test stopped
    - 10b: Test paused
    - 11b: Emergency Stopped
- The next bit indicates if the device has completed its initialization and is ready to receive commands and begin
  processing
- The next 4 bits indicate the currently set time between heartbeats, or zero for no heartbeats.

### Actuator State Packet

This packet is used to return the state of an actuator. This is 2 additional bytes, making the packet length 6. The
first byte indicates the device ID. The next byte indicates the state of the device:

- 0x00: The actuator is off
- 0x80: The actuator is on
- Any other data is treated as on

### Boolean Sensor Packet

This packet is used to return the state of a simple boolean sensor. This is 2 additional bytes, making the packet
total length 6. The first additional byte encodes the ID. The next byte indicates the device state:

- 0x00: A false reading
- 0x80: A true reading
- Any other data is also treated as true

### Custom Data

These packets are intended to allow the target and host to send arbitrary data to each other, and does not include a
timestamp. This packet is at least 1 additional byte, making the minimum length of the packet 1.

All bytes following the class byte are the raw bytes sent by the target. The length of bytes sent can be calculated
using the packet length field from the header byte. Because the header byte indicates the length, the maximum length of
the raw serial data that can be sent in one packet is 63 bytes.

---

All other sensors currently defined can be categorized into a few standard data packets, and support IDs:

### One float sensors

This packet can be used for sensors that send one single-precision (IEEE 754) float as data. This includes:

- Ambient Pressure (bars)
- Ambient Temperature (Celsius)
- Pressure Transducer (psi)
- Hygrometer (relative humidity, percentage)
- Load Cell (weight, kilograms)

with units specified in the list. Following the 4 timestamp bytes, this packet has 1 ID byte, then the float.

### Two float sensors

This packet can be used for sensors that send 2 single precision floats as data. This includes:

- Stepper Motor (absolute position (degrees), speed (degrees per second))
- Power Monitor (voltage, power (watts))

with units and order specified in the list. Following the 4 timestamp bytes, this packet has 1 ID byte, then the 2
floats.

### Three float sensor

This packet can be used for sensors that send 3 single precision floats as data. This includes:

- Accelerometer (x, y, z axes (meters per second per second))
- Gyroscope (x, y, z, axes (degrees per second))
- Magnetometer (x, y, z axes (Gauss))

with units and order specified in the list. Following the 4 timestamp bytes, this packet has 1 ID byte, then the 3
floats.

### Four float sensor

This packet can be used for sensors that send 4 single precision floats as data. This currently only includes GPS
data, which is ordered as longitude, latitude (degrees), altitude (meters above eclipse (HAE)), and ground speed
(meters per second).

---
The remaining class codes are currently undefined behavior.