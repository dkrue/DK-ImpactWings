# DK-ImpactWings
 LED wall art remotely controlled via DMX

_ImpactWings_ are a pair of 4 foot LED arms with ambient light animations that can be remotely controlled by any DMX stage lighting controller for an increasingly intense effect.

Unlike most of my other projects, this has no onboard controls. Everything is communicated by the DMX serial protocol and can be daisy-chained with other addressable lighting modules, such as professional stage lights.

![Impact Wings Wall Art](/images/final_result_leds.jpeg)

For this project I programmed a dozen ambient light animations drawn across 160 LEDs (10x 16 LED squares), which when controlled with a DMX controller can do cool strobing and other music reactive effects that make it fun to interact with.

## Origin
I was learning about how professional light shows are connected and programmed for concerts and festivals. It turned out they communicate via DMX, a sort of old cousin to MIDI, and these signals can be easily interpreted by an Arduino.

Having experience with so many MIDI projects, DMX interested me, especially as it would allow you to chain multiple LED light projects together and control them both separately and together.

## Goal
- Make a project able to receive real-time DMX serial communication. Extra credit: add a hardware DIP-switch to set the DMX device address.
- How many LEDs can a normal Arduino 16MHz _ATmega 328P_ chip animate smoothly?  I've done several LED projects with more powerful microcontrollers but I was curious to push what the classic Arduino could do.

![Controlling 2 Arduinos with DMX](/images/dmx_control_two_arduinos.jpeg)
An off-the-shelf DMX lighting controller sending data to two Arduinos, receiving data on separate channels through the same XLR cable. The finalized circuitry is also updating the animated LEDs.

## Challenges
### DMX / LED timing
DMX controllers send a stream of serial data out, such that the Arduino's serial interrupt routine will run about every 5ms.  The problem with this is the LEDs require about 11ms of continuous time to physically output the data to all LEDs, and need to be output at least every 33ms to achieve smooth animation at 30 frames per second. That leaves a 22ms window where we can listen for incoming DMX.

> Program Loop:
Calculate Animation Data > Send Data to LEDs > Listen For DMX > Process DMX packet > Stop Listening > Calculate Next Animation

### Electrical DMX Isolation
Professional lighting equipment is "isolated" meaning the circuitry controlling the lights and decoding the DMX are electrically isolated from one another. This blocks high voltage from the connected XLR cable from damaging the device.  The circuitry to achieve this was new to me but I learned how to implement a optoisolator IC for the signal and a galvanic DC isolator for the power.

![Final project circuit](/images/final_project_hardware.jpeg)
Final circuit with XLR input & output jacks and a DC power jack in a nice aluminum enclosure!

![Final prokject output jacks](/images/final_xlr_dc_jacks.jpeg)

## Software
- [FastLED](https://github.com/FastLED/FastLED/wiki/Overview) - My favorite Arduino library for controlling large numbers of addressable LEDs
- [Conceptinetics](https://sourceforge.net/p/dmxlibraryforar/wiki/Home/) - Arduino library for listening for DMX serial signals
- Digital pins read the device's DMX address from an onboard DIP switch on startup. This is a value from 1-512 set by binary switches.
- LED animation bitmaps are stored as byte arrays in Arduino PROGMEM (flash memory)

## Hardware
- [Adafruit Metro](https://www.adafruit.com/product/2590) (Arduino Uno 328p equivalent)
- MAX485 serial RS-485 decoder
- 10 pieces 16-LED WS2812b matrix squares (neopixel equivalent)
- XLR male and female jacks for DMX communication
- B0505s DC-DC 5V isolator, 6N137 optoisolator IC
- 5V 10A power supply
- Cheap chinese DMX light controller (eBay $40)

![Impact Wings Wall Art](/images/final_result_side.jpeg)