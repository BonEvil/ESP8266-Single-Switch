# ESP8266-Single-Switch
This .ino file sets up a generic Alexa/Wemo device with dynamic configuration. This is a compilation of other works from github users* as well as new code to enhance the setup experience.

# Motivation
Most other sketches that allow for IoT devices using the Alexa/Wemo paradigm require you to set up the client credentials and device name statically in the code. This presents a couple issues that bothered me.

1. Each new device would need to either have a separate .ino file created (copied) with the new device name or the file would have to be changed for each device that was to be added.
2. If for some reason the device location name or network SSID and password needed to be changed (I'll address this in a moment), the ESP8266 would need to be flashed again with the new information.

Firstly, I think we all agree that we don't like duplicate code. Especially where code maintenance is concerned.

Secondly, I ran in to an issue where I did not realize that the current draw from my garage lights exceeded the relay capacity. I decided to then use the already built device in another room. Luckily, I had already used this particular sketch on the device which made changing it to another room pretty trivial. Also, I had to change my 2.4Ghz network SSID one time because some other consumer device did not like the fact that it was similar to my 5Ghz network name. And I mean SIMILAR, not the SAME, yet it would try to connect to the 5Ghz instead of the 2.4Ghz where it would fail.

# Parts
For this device I am using the following:
- ESP8266 12E/F (they are basically the same)
- Single low current relay board (negative trigger)
- AC to 5v power supply (I'll explain why 5v and not 3.3v)
- 3.3v power converter
- 1000uF capacitor
- 0.1uF capacitor

The ESP8266 uses 3.3V as it could get damaged using 5v to power it. The problem I ran in to was that the AC to 3.3v only had 600mA of output and would constantly go in to protect when trying to power the ESP8266 and the relay board. I ended up using the 5v version (with 700mA output) and dropping it to 3.3v which then puts out slightly more than 700mA (I haven't officially measured) and is more easily able to run the system. The capacitors are for smoothing out the power as well as helping to keep noise out of the ESP8266 power supply.

# Setup
The ESP8266 requires 3.3v on the VCC and Ground on the GND to power the device. 3.3v is also supplied to the EN pin in order to power up the WiFi (non-sleep mode). Ground is also applied to the GPIO15 pin so that the device knows to use it's on-board memory to run the program.

In a typical home, a single switch (not a 3-way switch) is either open or closed. The ESP8266 needs to detect either HIGH or LOW on GPIO16 in order to determine which way the switch is positioned. Since the home switch is either open or closed, we need to add a resistor from GPIO16 to VCC to simulate the HIGH when the switch is in the OFF (open) position. One side of the switch is connected to Ground (GND) and the other to GPIO16. When the switch is in the ON (closed) position, GPIO16 gets the LOW signal.

GPIO5 is connected to the negative trigger of the relay board to either turn the light on or turn it off. The ESP8266 will get either an ON or OFF directive from Alexa that will directly translate to either turning the relay ON or OFF. If the light is already ON, sending a 'Turn on the xxxxxx light' will have no effect as the relay is already in the ON position. The switch, however, will simply change the relay state to it's opposite as the switch position is changed. Therefore, it is possible that the light could be ON whether the switch is UP or DOWN. It all depends on the current state of the relay.

The following shows the basic circuit diagram:

![Circuit](https://github.com/BonEvil/ESP8266-Single-Switch/raw/master/resources/ESP8266-Switch-and-Relay.png)

# Access Point
Since this requires no code configuration, it contains an access point for remotely setting up the device to connect to the local network as well as giving it a name for Alexa to recognize.

The access point initializes with the SSID 'ESP8266 Thing XXXX' where XXXX is the last two bytes (in HEX) of the MAC address to give it a somewhat unique name.

It is initially set up as an unsecured network for easy access. Just connect to it from the network settings on your computer or phone and then navigate to the root IP (192.168.4.1) which will render a page like the following:

![Root](https://github.com/BonEvil/ESP8266-Single-Switch/raw/master/resources/index.html.png)



**Disclaimer:**
I am in no way an expert on the ESP8266 or micro-electronics in general. However, I have started to experiment extensively with them. I am a mobile application developer professionally which has led me to working with many peripherals including Bluetooth LE scanners and dongles, card reader sleds, and others. I have now ventured in to the world of IoT and I'm having fun so...
