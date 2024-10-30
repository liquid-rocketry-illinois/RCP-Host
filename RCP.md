# LRI Rocket Control Protocol (RCP)

## Introduction

This document describes the protocol used with rockets, test stands, or other hosts in which low speed status messages
or manual control need to be communicated. This protocol is **not** designed to be used with high speed real time
control or data logging. These functions should be features of the host itself; periodic updates or occasional manual
control may be implemented using RCP.

This protocol does not perform error checking or acknowledgements. Depending on the physical media and lower layers used
to transmit this protocol, these features may already be available.

### Definitions

- Host: The host is the device or apparatus which is being tested, and contains the actuators and sensors in question,
  as well as the microcontroller which interfaces between this protocol and the sensors
- Controller: This is the computer or device sending control packets. This is likely the computer running the desktop
  software, but it can be any device that sends control packets. The host will only send data back to the controller
  when requested, including streaming data.
- Control packet: A packet sent from the controller to the host that contains information requesting an update to the
  host state.
- Data packet: A packet sent from the host to the controller, containing either the data requested from a control
  packet, or a data point from a stream of data.
- Data streaming: A host state in which data about sensors and actuators is sent to the controller continuously to be
  logged or visualized on the controller side. This streaming is not meant to be the primary form of data collection. It
  is only so the controller can receive periodic updates on the host status.
- Header Byte: The first byte of a packet. It indicates the channel being communicated on, and the length of the rest of
  the packet.
- Class Byte: The second byte of a packet. It indicates the packet "class," or the functionality of the rest of the
  transmitted data.
- Packet length: The total length of the packet, not including the header byte.

## Packet Structure

Both data and control packets share the first two bytes.

### Header Byte

The header byte is the first byte of any packet. This contains information on both the channel of communication, and the
subsequent length of data being sent.

#### Channel Bits

The first two bits of the header byte indicate the channel of communication. These 4 channels (00, 01, 10, 11) allow for
multiple host-controller setups to be in use at the same time, without interfering with each other. For a host and
controller to communicate, each should use and respond to the same channel bits.

Hosts and controllers on other channels should not interact with packets from other channels. Each channel is intended
to support only one host and one controller, but implementations with multiple hosts that do not send data packets are
possible.

#### Length Bits

The remaining 6 bits of the header byte are used to indicate the length of the packet following the header byte,
including the class byte (specified below).

A length of 0 is reserved for emergency stops. If a host receives a packet with length zero at any time, it should
immediately enter an emergency stop state. This exact state is dependent on the host and it's physical structure, and
when receiving this packet it should complete whatever emergency protocols it has.

Any other length from 1 to 63 specifies the length of the packet excluding this header byte.

### Class Byte

This byte indicates the purpose of the rest of the packet. The exact function and definition of the class byte depends
on if the packet is for control or for data. This byte is included in the length field of the header byte.

## Control Packets

Control packets are sent from the controller to the host to request an update to the host state, or to request data from
the host. The function of the control packet is determined by the value of the class byte.

The meanings of the class byte are as follows:

- 0x00: Testing commands
- 0xF0: Testing query
- 0x01: Solenoid write request
- 0xF1: Solenoid read request
- 0x02: Stepper motor write request
- 0xF2: Stepper motor read request
- Remaining codes are not yet defined and can be used as needed.

### Testing Command

A testing command is a request to change the state of a test. This includes starting, stopping, pausing, and controlling
data streaming. The testing command includes 1 additional byte (for a total length of 3). This additional byte specifies
which testing function to perform:

- 0x0x: This indicates a request to begin a test. The first 4 bits of this testing command must all be zero, but the
  remaining 4 bits allow for a test number to be selected. This allows for the host to have multiple test sequences
  preprogrammed, and the controller can dynamically select which test to start without requiring new code to be uploaded
  to the host.
- 0x10: Full stop the currently running test
- 0x20: Pause/Unpause the currently running test
- 0x30: Start data streaming (Full details in data packet section)
- 0x40: Stop data streaming
- Remaining codes are not yet defined and can be used as needed.

### Testing Query

A testing query is a request for the current state of a test. This request needs no additional bytes, so the packet
length field should be set to 1. The host should respond with the appropriate data packet.

### Solenoid Write Request

This packet class is a manual write request to a solenoid. This command contains one additional byte, making the total
packet length 2. The additional byte encodes both the state requested, and the solenoid ID to write to. The first 2 bits
indicate the requested state:

- 01b: Turn solenoid on
- 10b: Turn solenoid off
- 11b: Toggle solenoid state

The remaining 6 bits indicate the solenoid ID to write to. The host is responsible for parsing this ID and translating
it to the appropriate hardware address.

### Solenoid Read Request

This packet class is a request for the state of a solenoid. This command contains one additional byte, making the total
packet length 2. The additional byte encodes only the solenoid ID to read from. The first 2 bits of this byte are
unused, and the remaining 6 bits encode the solenoid ID. The host should respond with the appropriate data packet.

### Stepper Motor Write Request

This packet class is a manual write request to a solenoid. This command contains 5 additional bytes; 1 ID byte and 4
value bytes. This makes the packet length 6. The first byte encodes both the requested function and the ID of the
stepper motor addressed. The first 2 bits indicate the function:

- 00b: Absolute positioning
- 01b: Relative positioning
- 10b: Speed based control
- 11b: Unused

The remaining 6 bits of the first byte are used to indicate the stepper motor to address. The host is responsible for
parsing this ID and translating it to the appropriate hardware address.

The remaining 4 bytes of information are the value for the indicated function. Depending on the hardware implementation,
these bytes may be a 32 bit integer, or a floating point number, or another representation not mentioned.

### Stepper Motor Read Request

This packet class is a request for the state of a stepper motor. This command contains one additional byte, making the
total packet length 2. The additional byte encodes only the stepper motor ID to read from. The first 2 bits of this byte
are unused, and the remaining 6 bits encode the solenoid ID. The host should respond with the appropriate data packet.

---
The remaining class codes are currently undefined, and can be used for future expansions.

## Data Packets

These packets are used to communicate state information from the host to the controller. Data logging packets begin with
a header byte and a class byte like control packets, but class byte values indicate different functions than class bytes
in a control packet. The class byte represents these data options:

- 0x00: Test state information
- 0x01: Solenoid state information
- 0x02: Stepper motor information
- 0x43: Pressure transducer data
- 0x80: GPS data
- 0x81: Magnetometer data
- 0x82: Pressure data
- 0x83: Temperature data
- 0x84: Accelerometer data
- 0x85: Gyroscope data
- 0xFF: Raw Serial Data

Classes with a most significant bit of zero indicate the sensor reading is for the host as a whole, for example a
general temperature reading, or acceleration reading (this is not an explicit requirement, more of a suggestion for
organization). If a host has multiple sensors for a specific type of data, it is the host responsibility to determine
which is the best data to send to the controller.

Data packets include a 4 byte time stamp immediately following the class byte. This timestamp indicates the milliseconds
since the test was started. This value **does not indicate absolute time in any way, and is not intended to be used for
accurate data logging**. This time value is only for the controller to be able to determine the rough time a data point
was produced for data visualization.

### Test State Packet

This packet is used to return the state of a test to the controller. This includes 1 additional byte, making the packet
length 6. This additional byte includes several pieces of information:

- The most significant bit indicates if data is currently streaming (1) or not (0)
- The next bit is not used
- The next two bits indicate test state:
    - 00b: Test running
    - 01b: Test stopped
    - 10b: Test paused
    - 11b: Emergency Stopped
- The next 4 bits indicate the test number that is currently selected

### Solenoid State Packet

This packet is used to return the state of a solenoid. This is only 1 additional byte, making the packet length 6. This
additional byte encodes both the state of the solenoid and the solenoid ID.

- The most significant bit is not used
- The next bit indicates if the solenoid is on (1) or off (0)
- The next 6 bits indicate the solenoid ID the data is from

### Stepper Motor State Packet

This packet is used to return the state of a stepper motor. This is 9 additional bytes, making the packet length 14.

The first byte indicates the stepper motor ID. The first 2 bits are unused, and the next 6 bits indicate the stepper
motor ID. The next 4 bytes are a signed integer that encodes the stepper motor position, in millionths of degrees.
The final 4 bytes are a signed integer that encodes the stepper motor speed, if available, in millionths of degrees per
second.

### Pressure Transducer Data

This packet is used to return the value of a pressure transducer. This is 5 additional bytes, making the packet length

10. The first byte indicates the transducer ID. The first 2 bits are unused, and the next 6 bits indicate the transducer
    ID.

The next 4 bytes are a signed integer representing the transducer measurement, in microbars.

### GPS Data

This packet is used to return GPS data. This is 32 additional bytes, making the packet length 37.

The first 8 bytes are a signed integer representing the latitude of the host, in millionths of degrees.

The second 8 bytes are a signed integer representing the longitude of the host, in millionths of degrees.

The next 8 bytes are a signed integer representing the altitude of the host, in millimeters above the ellipsoid (HAE).

The final 8 bytes are a signed integer representing the ground speed of the host, in millimeters per second.

### Magnetometer Data

This packet is used to return magnetometer data. This is 12 additional bytes, making the packet length 17.

The first 4 bytes are a signed integer representing the magnetic field in the X direction, in millionths of
Gauss.

The next 4 bytes are a signed integer representing the magnetic field in the Y direction, in millionths of
Gauss.

The final 4 bytes are a signed integer representing the magnetic field in the Z direction, in millionths of
Gauss.

### Pressure and Temperature Data

These packets are used to return ambient pressure and temperature data. Both packets are 4 additional bytes of data,
making the packet length 9. These bytes are a signed integer, representing the value in millionths of degrees Celsius,
or in microbars respectively.

### Accelerometer and Gyroscope Data

These packets are used to return overall acceleration and rotation data. Both packets are 12 additional bytes of data,
making the packet length 17.

The first 4 bytes are a signed integer, representing acceleration/rotation in the X axis, in millimeters per
second per second or degrees per second, respectively.

The next 4 bytes are a signed integer, representing acceleration/rotation in the Y axis, in millimeters per
second per second or degrees per second, respectively.

The last 4 bytes are a signed integer, representing acceleration/rotation in the Z axis, in millimeters per
second per second or degrees per second, respectively.

### Raw Serial Data

These packets are intended to allow the host to send arbitrary serial data back to the controller. This packet is at
least 1 additional byte, making the minimum length of the packet 6.

All bytes following the timestamp bytes are the raw bytes sent by the host. The length of bytes sent can be calculated
using the packet length field from the header byte. Because the header byte indicates the length, the maximum length of
the raw serial data that can be sent in one packet is 58 bytes.

---
The remaining class codes are currently undefined, and can be used for future expansions.
