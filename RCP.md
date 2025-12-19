
# LRI Rocket Control Protocol (RCP) v2.0.0

# Introduction

This document describes the protocol used with rockets, test stands, or other targets for sending status change requests or receiving telemetry from target state and device readouts.

This protocol does not perform error checking or acknowledgements. Depending on the physical media and lower layers used to transmit this protocol, these features may already be available.

## Definitions

- Target: The target is the device or apparatus which is being tested, and contains the actuators and sensors in question, as well as the microcontroller which interfaces between this protocol and the sensors
- Host: This is the computer or device sending control packets. This is likely the computer running the desktop software, but it can be any device that sends control packets. The target will only send data back to the host when requested, including streaming data
- Compact Packet: Original packet type, used for single information unit packets
- Extended Packet: A packet allowing for longer lengths. This enables longer string messages, as well as amalgamating multiple information units into a single packet
- Header Byte(s): The first byte of a compact packet, or the first 3 bytes of an extended packet. It indicates the channel being communicated on, and the length of the rest of the packet
- Class Byte: A single byte which indicates the functionality of the following parameter bytes
- Parameter Bytes: The bytes in a packet that follow the class byte.
- Information unit: The combination of the class and its associated parameter bytes. This is the smallest amount of information that will be sent in a packet. There may be multiple information units in one packet (i.e. an [amalgamation unit](#amalgamation-units))
- Packet length: The length of the packet, defined slightly differently for compact and extended packets
- ID: A unique 8-bit identifying number within a device class to identify an individual device. IDs range from 0 - 255
- Device: An actuator, sensor, or other component (including software/virtual state components) on a target that can be controlled, queried, or otherwise interacted with. All devices can be fully qualified with its device class and ID
- Fully Qualified Device Name (FQDN): The 16 bit identifier that makes up a unique number that identifies a device on the target. The first 8 bits indicate the device class (shared with the Class Byte), the next indicate a device ID. Two devices may share the same ID, as long as they are not under the same device class.
- Interface: The method by which the host and target communicate with each other. For example, LoRa, USB, TCP/IP over various methods, etc.
- Float value: IEEE754 floating point values
- Test: An automated sequence of actions executed by the target. Tests are identified via an 8-bit ID value. Also referred to as auto-sequences
- Data Channel: some devices return multiple types of data at a time. For example, the GPS returns latitude, longitude, altitude, and ground speed all at the same time. Each one of these is an individual data channel. Data channels are zero indexed.

## Device Classes

A device class is an 8 bit identifier which encodes the type of device, and the functionality of a packet. For example: a solenoid, a sensor, a software component, etc. Device classes, combined with a device ID number (if relevant), form the FQDN and can be used to uniquely identify an individual device on a target. Some devices, such as the test state, do not require IDs, since there can only be one such device present.

The defined device classes are as follows:
- `0x00`: [Test State](#test-state) +*%
- `0x01`: [Simple Actuator](#simple-actuator) *%
- `0x02`: [Stepper Motor](#stepper-motor) *%
- `0x03`: [Prompt Input](#prompt-input) +&^%
- `0x04`: [Angled Actuator](#angled-actuator) *%
- `0x80`: [Target Log](#target-log) ^%
- `0x90`: [Ambient Pressure](#xf-units) *
- `0x91`: [Temperature](#xf-units) *
- `0x92`: [Pressure Transducer](#xf-units) *
- `0x93`: [Hygrometer](#xf-units) (relative humidity) *
- `0x94`: [Load Cell](#xf-units) (weight) *
- `0x95`: [Boolean Sensor](#boolean-sensor) *%
- `0xA0`: [Power Monitor](#xf-units) *
- `0xB0`: [Accelerometer](#xf-units) *
- `0xB1`: [Gyroscope](#xf-units) *
- `0xB2`: [Magnetometer](#xf-units) *
- `0xC0`: [GPS](#xf-units) *
- `0xFF`: [Amalgamate Unit](#amalgamation-units) +^%

Entries marked with `+` are virtual devices with no ID available. This means that these devices don't have a physical representation on the target itself. Rather, this is purely software, but is still a component that can be queried for information and updated to perform actions on the target.

Entries marked with `*` can be [amalgamated](#amalgamation-units). This means that their information units can be batched into a single packet to reduce the number of discrete things that need to be sent over the interface.

Entries marked with `&` are information units that are not timestamped.

Entries marked with `^` cannot be queried.

Entries marked with `%` cannot be tared.

As an aside, devices are loosely assigned class numbers based on a few things. First, all read-only devices have the MSB set. The next 3 bits for read-only's indicate how many data channels the device has. For example, GPS returns the latitude, longitude, ground speed, and altitude, thus it has 4 data channels. From there the order is simply numerical. This is not a strict standard, only a convention that will (hopefully) be maintained.

# Packet Structure

There are two packet formats that are used for sending different amounts of data. Both packet types begin with the same 2 bits:

#### Channel Bit

The first bit of the header byte indicates the channel of communication. These 2 channels (`0` or `1`) allow for up to 2 target-host setups to be in use at the same time, without interfering with each other. For a target and host to communicate, each should use and respond to the same channel bit.

Targets and hosts on one channel should not interact with packets from the other. Each channel is intended to support only one target and one host, but implementations with multiple targets that purely receive state are permitted.

#### Packet Format Bit

This bit indicates whether the extended or compact packet format is used for the rest of the packet. A `0` bit indicates compact, and `1` indicates the extended format.

After these first 2 bits, the remaining packet format is determined by the format type.

## Compact Packet Format

The compact format is the original RCP definition. This packet is restricted to 63 parameter bytes in length. The compact packet begins with the required channel bit and a format bit set to `0`. The remaining 6 bits in the first byte of the packet indicate the length of the packet.

A compact packet of length of 0 is reserved for emergency stops. If a target receives a compact packet with length zero at any time, it should immediately enter an emergency stop state. This exact state is dependent on the target and it's physical    
structure, and when receiving this packet it should complete whatever emergency protocols it has. An emergency stop packet contains only this header byte and no other bytes, and as such the only valid emergency stop packet is `0bx0000000`, where `x` indicates the channel. A host receiving an emergency stop packet has no meaning, so any zero-length compact packets received by the host should be discarded.

Any other length from 1 to 63 specifies the length of the packet excluding this header byte and the following class byte.

## Extended Format

The extended packet format is used when larger chunks of data need to be transferred, including batching sensor/target status data into a single packet to reduce the number of discrete things that need to be sent.

Extended packets begin with the appropriate channel bit and the format bit set to `1`. The remaining 6 bits in the first header byte are unused, and should be set to zero for all communications. The next 2 bytes indicate the length of the packet as an unsigned 16-bit integer, big endian. After these 3 bytes come the information unit.

The 16-bit packet length only includes the number of parameter bytes minus one; the class byte is **not** included, and to determine the actual number of parameter bytes the length must be incremented by 1. This is so such that a length of 0 has a meaning; in this case it will mean there is 1 parameter byte. A length of 1 means 2 parameter bytes, length of 2 means 3, and so on. This means the maximum number of parameter bytes for an extended packet is 65536 bytes.

Other than the way packet length is signaled, there is no other difference between the extended and compact format. Either format can be used for transmitting an information unit that could fit in a compact packet. Extended packets cannot be sent from the host.

## Overflows

While it is out of the scope of RCP as this protocol would be an OSI application layer protocol and is not concerned with transport or guaranteeing delivery, overflows of receiving buffers still need standard behavior. If a buffer would overflow on packet receiving, the next-up packet in the receiving buffer should be popped off, unless it is an emergency stop packet. In this case, the incoming packet should be discarded.

# Information Units

This section describes the format for the valid information units.

All information units (with a few exceptions as noted above) are timestamped by the target before being sent over the interface. The timestamp is a 4 byte big endian value indicating the number of milliseconds since the target's epoch, and comes directly after the class byte.

In an amalgamation unit, only the base amalgamation information unit is timestamped, and all sub-units will inherit this timestamp. This is described more in the [amalgamation unit](#amalgamation-unit) section.

When requesting a state change (for example, writing to an actuator), the target will respond with the read request packet for that device type with the new state of the actuator. This allows the host to know if the change was successfully implemented in actuality, or if something prevented the change from taking place.

Unless otherwise noted, a device can be read from with an information unit containing just the class and the device ID. Devices which don't have an ID have unique methods of querying.

## Test State

The test state virtual device represents all the information about the general state of the target. Reading and writing to/from this device involves things like test information, controlling data streaming, heartbeats, and other meta-information.

### Writes

Writing to the test state device controls some target-level state. The first parameter byte can be one of the following:
- `0x00`: This indicates a request to start a test. A second parameter byte follows this, containing the test ID to start
- `0x10`: Full stop the currently running test
- `0x11`: Pause/Unpause the currently running test
- `0x12`: Request the device performs whatever hardware reset routine it has
- `0x13`: Request the device reset its time epoch to the current time (i.e. timestamps will be restarted at zero)
- `0x20`: Stop data streaming
- `0x21`: Begin data streaming
- `0x30`: Query current test state
- `0xF0`: Set heartbeat interval. A second parameter byte follows this, containing the allowable time between heartbeats in hundreds of milliseconds. A value of `0` indicates disabling heartbeats.
- `0xFF`: Heartbeat packet
- The remaining formats are reserved and are undefined behavior

#### Heartbeat Specifics

When heartbeats are enabled, both the host and target shall expect to receive a heartbeat from the other at least as often as the specified interval. If the target does not receive a heartbeat from the host in the specified interval, it should assume communications to the host have been lost irrecoverably, and the programmed emergency stop procedure should be activated. It is up to the host to decide what to do on loss of heartbeats.

### Response

On a query of the test state, the target will send back an information unit with the following parameter bytes:
- The most significant bit of the first byte encodes if data streaming is enabled (`1`) or not (`0`)
- The next two bits indicate test state:
  -   `0b00`: Test running
  -   `0b01`: Test stopped
  -   `0b10`: Test paused
  -   `0b11`: Emergency Stopped
- The next bit indicates if the device has completed initialization and is ready to receive instructions (`1`) or not (`0`)
- The next 4 bits are unused
- The next byte contains the configured heartbeat interval
- The next byte contains the ID of the currently running test *
- The next byte contains the test progress value. This is an arbitrary value between zero and 255 indicating the progress of the running test. This is for the host to be able to display test progress, and does not have any further meaning. \*

\* The last 2 bytes of the response are not sent if the target is in the Test Stopped state.

### Examples

- `0x02 00 00 05`: start test with ID 5
- `0x01 00 21`: enable data streaming
- `0x01 00 90 0A 05 0A`: a response packet indicating data streaming is enabled, the device has initialized, the heartbeats are expected once per second, and the device is running test 5 and is at stage 10

## Simple Actuator

A simple actuator is a device on the target which has an on and off state. This is generalized to all actuators of this type, such as solenoids, or simple on/off lights, etc. This device follows the standard read request format.

### Writes

A simple actuator can be written to with the following 2 parameter bytes:
- The device ID
- Set point:
  - `0x00`: Off
  - `0x80`: On
  - `0xC0`: Toggle

### Response

Besides the timestamp (if required), the parameter bytes for this class contains 2 bytes:
- The ID of the device
- The state:
  - `0x00`: Off
  - `0x80`: On
  - Any other bit sequence is not allowed and is undefined behavior

### Examples

- `0x01 01 00`: Requests state of simple actuator with ID `0`
- `0x02 01 01 C0`: Requests simple actuator with ID `1` toggle its current state
- `0x06 01 00 00 00 FF 02 80`: A response packet indicating that at time `255`ms, simple actuator with ID `2` was in the On state

## Stepper Motor

The stepper motor class refers to any kind of rotational actuator that has individual speed and angle control (thus most likely a stepper). For a more generalized angled component, see the [angled actuator](). This device follows the standard read request format.

### Writes

Stepper write requests follow this format:
- First byte contains the ID
- Second byte is the control mode:
  - `0x40`: Absolute positioning (degrees)
  - `0x80`: Relative positioning (degrees)
  - `0xC0`: Speed control (degrees/second)
  - The next 4 bytes are a float value containing the set point for the specified control mode

### Response

This device responds with a [2F](#xf-units) packet.

### Examples

- `0x06 02 01 40 41 8e 80 00`: Set stepper `1` to an absolute position of `17.8125` degrees

## Prompt Input

The prompt device can be used by the target to ask the host for input at runtime. Since it is a software component, it does not have an ID, prompt information units are not timestamped, and cannot be amalgamated. As of writing, prompts can be used for either float inputs or go-no go authorization, but it has room to expand to other types. When prompting for input, the target will send the following information unit:
- The first byte encodes the prompt type:
  - `0x00`: Go-No Go authorization (boolean value)
  - `0x01`: Float input
  - `0xFF`: clear active prompt. This prompt type does not include a prompt string
- The remaining bytes in the packet are the prompt string and can be used by the host to include a string message describing the prompt. Each byte is a single ASCII character, and the string is **not null terminated**. The length of the string must be calculated from the length field of the packet

The type byte indicates to the host what kind of data the target is expecting to receive. Only 1 prompt can be active at a time, and the host should respond to the target with the data type of the latest prompt request. Depending on the type, the host should respond with the appropriate information unit:
- Go-No Go (boolean) prompt:
  - If go: `0x01`
- If No Go: `0x00`
- All other bit sequences are undefined behavior
- Float input:
  - 4 bytes with the float value
- Clear active prompt: This type does not expect a return value. It is instead a request from the target that the host discards the last prompt request. No error should occur if there is no active prompt.

The prompt input device class cannot be queried, and if the host sends a packet to this device without having first received a prompt request (or an incorrect data type is sent back) then the packet sent by the host should be ignored by the target.

### Examples
- `0x11 03 01 45 6E 74 65 72 20 61 20 6E 75 6D 62 65 72 3A 20`: A float request with prompt string "Enter a number:"
- `0x04 03 41 8e 80 00`: A response to the above prompt with value `17.8125`

## Angled Actuator

An angled actuator refers to any device which controls the angle of something, such as a servo, motor shaft, aileron, etc. and does not need anything beyond angle control. This device follows the standard read request format.

### Writes

To write to an angled actuator, use the following format:
- The first byte contains the ID of the device to write to
- The next 4 bytes contain a float encoding the absolute rotation to set the actuator to (degrees)

### Response

This device responds with a [1F](#xf-units) packet.

### Examples

- `0x05 04 01 41 8e 80 00`: Sets actuator `1` to angle `17.8125` degrees

## Target Log

This device allows the target to send logging messages back to the host. Following the 4 byte timestamp, the rest of the packet is ASCII chars encoding the message. The string is **not** null terminated, so the length must be calculated from the packet length.

### Example

- `0x18 80 00 00 00 FF 5B 49 4E 46 4F 5D 3A 20 48 65 6C 6C 6F 20 57 6F 72 6C 64 21`: This encodes a string containing "[INFO]: Hello World!" send at time `255`ms

## Boolean Sensor

A boolean sensor is one such that it is either on or off. This device follows the standard read request format.

### Response

When queried, the target will respond with the following information unit:
- The first byte encodes the device ID
- The next byte encodes the reading:
  - `0x00`: false
  - `0x80`: true
  - Any other bit sequence is undefined behavior

## xF Units

xF units are a general format of response that include `x` number of floating point values. The currently in use xF units are the 1F, 2F, 3F, and 4F for devices that have 1, 2, 3, or 4 data channels.

The remaining unspecified device classes (except amalgamation) all accept the standard read request, and will respond with the appropriate xF unit as mentioned below.

Each xF unit follows this format, after the class and timestamp:
- The first byte indicates the ID
- The next 4 bytes encode the first float
- The next 4 bytes encode the second float
- So on and so forth

xF units lend themselves well to amalgamation, as specified under the [amalgamation unit](#amalgamation-unit) section.

The 1F devices include:
- Angled Actuator (degrees)
- Ambient Pressure (bars)
- Temperature (Celsius)
- Pressure Transducer (PSI)
- Hygrometer (% relative humidity)
- Load Cell (kg)

The 2F devices include, with the data channels in the order specified:
- Stepper Motor: absolute position (degrees), speed (degrees/second)
- Power Monitor: voltage, power (watts)

The 3F devices include, with the data channels in the order specified:
- Accelerometer: x, y, z (meters/second/second)
- Gyroscope: x, y, z (degrees/second)
- Magnetometer: x, y, z (gauss)

The 4F devices include, with the data channels in the order specified:
- GPS: latitude, longitude (degrees), altitude (meters above eclipse (HAE)), ground speed (meters per second)

### Examples

- `0x11 C0 00 00 00 05 00 41 8e 80 00 3F 80 00 00 40 00 00 00 40 40 00 00`: A GPS response from GPS with ID `0` at time `5`ms, indicating the target is at latitude `17.8125` degrees, longitude `1` degree, altitude `2`m, and ground speed `3`m/s
- `0x05 92 00 00 00 05 06 40 00 00 00`: A PT response from PT `6` at time `5`ms, reading a pressure of `2`PSI

## Amalgamation Units

Amalgamation units are a way to include multiple of the above information units into a single packet, so that data from the target can be batched into one discrete thing to send, rather than a collection of smaller individual packets. Only certain device classes support amalgamation, but any number of information units can be included.

All information units included in an amalgamation unit will all share the same timestamp; as such the normal timestamp bytes are not included in the individual information units like normal, rather it is encoded at the beginning of the amalgamation unit itself.

An amalgamation unit begins with the appropriate header, and the class byte `0xFF`. Following these, the 4 byte timestamp is attached. From there, the sub-information units are appended one after the other. All information units that can be amalgamated have a definite or otherwise calculable length, so there is no direct indication of the number of sub-units included in an amalgamation unit. Rather, the total length of the packet and the number of bytes already processed must be used to determine when there are no more sub-units to process.

An example of a packet with an amalgamation unit looks as follows:  
`0x27 FF 00 00 00 FF | 90 00 40 00 00 00 | 92 00 40 00 00 00 | 92 01 40 40 00 00 | 95 00 80 | B0 00 3F 80 00 00 40 00 00 00 40 40 00 00`

The bars deliminate the individual sub-units contained in this amalgamation unit. One can see the timestamp of `255`ms at the very beginning following the class byte, and how none of the sub-units have a timestamp like they would when non-amalgamated. This packet includes information from ambient pressure sensor `0`, PTs `0` and `1`, boolean sensor `0`, and from accelerometer `0`.

This packet could also have been encoded in the extended format:  
`0x40 00 26 FF 00 00 00 FF | 90 00 40 00 00 00 | 92 00 40 00 00 00 | 92 01 40 40 00 00 | 95 00 80 | B0 00 3F 80 00 00 40 00 00 00 40 40 00 00`

Note that an amalgamation unit cannot be a sub-unit of another amalgamation unit, and amalgamation units cannot be sent from the host to the target. They are only for reading data, not for writing.

## Tare Requests

A range of devices can be requested to tare to a given value. The tare information unit is as follows:
- The first byte contains the device ID
- The next byte contains the data channel to tare
- A float value, indicating by what value the data stream should be offset by. This value should be **added** to all future values sent by the device. If multiple tare requests are received over the runtime of a target, these values can simply be added together (i.e. tares are relative to the offset data stream, not to the raw stream itself)

The devices which support tares are the following:
- Ambient Pressure
- Temperature
- Pressure Transducer
- Hygrometer
- Load Cell
- Power Monitor
- Accelerometer
- Gyroscope
- Magnetometer
- GPS

# Standard Read Request Examples

Many devices will follow an identical format for read requests, for simplicity. The information unit for such a request contains the class byte and the ID byte of the device to read from.

### Examples

- `0x01 B1 0F`: Read from gyroscope `15`
- `0x01 94 02`: Read from load cell `2`
- `0x01 04 00`: Read from angled actuator `0`

---  

All other class codes are reserved and are undefined behavior if received.
