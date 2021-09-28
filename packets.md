
# MK4 Packet Structure

* Raw Human Interface Device packets are limited to 64 bytes.
* In order to facilitate multiple clients talking to the firmware at once, each packet must be accompanied by an ID that's unique to the client.
* The firmware doesn't perform verification pertaining to the validity of ID's provided by clients nor does it take care to verify that a client STR_ID within the initiation sequence is actually unique. These ID's are simply for organizational purposes.
* The client must maintain a list of it's sent packet ID's and keep track of which ones are ackknowledged by reading through ALL packets that are sent back from the chip. It's similar to TCP internals.
* Teensy 3.2 EEPROM is 2048 bytes long has theoretically limited write capacity so it's important to be easy with writing to it.

## Header

The first two bytes of any packet related to Sim Coaches are:

```
['S']['C']
```

That being the ASCII values of the characters S and C.

# Firmware

## Initiation

The client needs to get a communications ID first. This is used to differentiate itself from other endpoints on the PC.

```
I ['S']['C']['!'][58_BYTE_STR_ID]
O ['S']['C']['#'][U16_ID][58_BYTE_STR_ID]
```

* The STR_ID should be a unique string of characters that the client can use to identify this packet in order to not be confused by other initiation reponse packets that are being sent out at the same time. A copy of it will be sent back in the reponse. It's safe to assume it's null-terminated since the last byte of the packet will always be zeroed.
* The ID is an unsigned 16-bit integer.

## Get Firmware Version

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['V']
O ['S']['C'][U16_ID][U16_PACKET_ID][U16_MAJOR][U16_MINOR][U16_REVISION]
```

* The firmware version is represented in 3 parts: Major.Minor.Revision (i.e. 4.0.0)
* The parts are unsigned 16 bit integers.
* The MK4 firmware always returns major: 4
* "Revision" pertains to code differences that do not affect compatibility with older "Minor" versions in terms of the communications protocol.
* "Minor" pertains to code differences to that affect compatibility with older versions.
* "Major" pertains to code differences that are vast and retain no hope of compatibility.

## Get EEPROM Size

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['S']
O ['S']['C'][U16_ID][U16_PACKET_ID][U16_SIZE]
```

## Get EEPROM Byte

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['E']
O ['S']['C'][U16_ID][U16_PACKET_ID][U8_VALUE]
```

# Axes

## Get Axis Count

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['J']['A']['C']
O ['S']['C'][U16_ID][U16_PACKET_ID][U8_COUNT]
```

## Get Axis State

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['J']['A']['S'][U8_AXIS]
O ['S']['C'][U16_ID][U16_PACKET_ID][U8_AXIS][BOOL_ENABLED][S8_CURVE][U16_MIN][U16_MAX][U16_VALUE]
```

* BOOL_ENABLED is true if the axis is functional at all.
* U8_CURVE signals whether or not the axis U16_VALUE is being modified by an input curve. If it's 0 then the value will the raw value read from the sensor. Otherwise it's the ID of the curve as stored within the EEPROM.
* U16_MIN and U16_MAX represent the low and top end of what is meant to be the valid range of input as per calibration.

## Set Axis Enabled

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['J']['A']['E'][BOOL_ENABLED]
O ['S']['C'][U16_ID][U16_PACKET_ID][BOOL_ENABLED]
```
## Set Axis Range

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['J']['A']['R'][U16_MIN][U16_MAX]
O ['S']['C'][U16_ID][U16_PACKET_ID][U16_MIN][U16_MAX]
```

## Set Axis (Bezier) Curve

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['J']['A']['B'][U8_CURVE]
O ['S']['C'][U16_ID][U16_PACKET_ID][U16_MIN][U16_MAX]
```

# Bezier Curve

## Get Info

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['B']['I']
O ['S']['C'][U16_ID][U16_PACKET_ID][U8_COUNT][U8_ELEMENT_COUNT]
```

* Teensy 3.2 MK4: 10 curves, 6 elements

## Set Curve Label

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['B']['S'][U8_CURVE][55_BYTE_STR]
O ['S']['C'][U16_ID][U16_PACKET_ID][U8_CURVE]
```

* 55 byte maximum string length.

## Get Curve Label

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['B']['G'][U8_CURVE]
O ['S']['C'][U16_ID][U16_PACKET_ID][55_BYTE_STR]
```

* 55 byte null-terminated string.

## Get Curve Element

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['B']['C'][U8_CURVE][U8_ELEMENT]
O ['S']['C'][U16_ID][U16_PACKET_ID][U8_CURVE][U8_ELEMENT][FLOAT_VALUE]
```

## Set Curve Element

```
I ['S']['C'][U16_ID][U16_PACKET_ID]['B']['C'][U8_CURVE][U8_ELEMENT][FLOAT_VALUE]
O ['S']['C'][U16_ID][U16_PACKET_ID][U8_CURVE][U8_ELEMENT][FLOAT_VALUE]
```