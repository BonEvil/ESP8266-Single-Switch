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

In a typical home, a single switch (not a 3-way switch) is either open or closed. The ESP8266 needs to detect either HIGH or LOW on GPIO4 in order to determine which way the switch is positioned. Since the home switch is either open or closed, we need to set GPIO4 as INPUT_PULLUP to keep the pin HIGH until GND is applied to the pin changing it to LOW as the other side of the light switch will be tied to GND.

GPIO5 is connected to the negative trigger of the relay board to either turn the light ON or OFF. The ESP8266 will get either an ON or OFF directive from Alexa that will directly translate to either turning the relay ON or OFF. If the light is already ON, sending a 'Turn on the xxxxxx light' will have no effect as the relay is already in the ON position. The switch, however, will simply change the relay state to it's opposite as the switch position is changed. Therefore, it is possible that the light could be ON whether the switch is UP or DOWN. It all depends on the current state of the relay.

The following shows the basic circuit diagram:

<img src="https://github.com/BonEvil/ESP8266-Single-Switch/raw/master/resources/ESP8266-Switch-and-Relay.png" style="width:300px" />

# Access Point
Since this requires no code configuration, it contains an access point for remotely setting up the device to connect to the local network as well as giving it a name for Alexa to recognize.

The access point initializes with the SSID 'ESP8266 Thing XXXX' where XXXX is the last two bytes (in HEX) of the MAC address to give it a somewhat unique name.

It is initially set up as an unsecured network for easy access. Just connect to it from the network settings on your computer or phone and then navigate to the root IP (192.168.4.1) which will render a page like the following:

<img src="https://github.com/BonEvil/ESP8266-Single-Switch/raw/master/resources/index.html.png" style="width:300px" />

Now just fill in the form and submit.

**Access Point SSID**
This is what you want advertised as the access point name, but more importantly, how you want Alexa to recognize the device. i.e. Living Room Light or The Party _(I have one hooked up to a disco light)_

Then you can say "Alexa, turn on the party" and she will respond appropriately.

**Access Point Password**
This is the password that you will need to connect to the new access point. I made it mandatory because.... security. Must be at least 8 characters since the ESP8266 doesn't like it less than that when set up as an access point.

**Station SSID**
This is the network SSID that you want the device to connect to.

**Station Password**
I'm assuming that you have a secured network as it won't allow an empty string to pass validation.

Speaking of validation, I didn't do any robust validation on the form. I may add some later. But as a guideline, do not use : (colon) or ; (semicolon) in any of the inputs. I am saving the network variables delimited by the : and ending with the ; in the EEPROM. So in order to retrieve them successfully, don't do it.

_**Reset**_

The Reset button will clear out the EEPROM and set the ESP8266 back to the default SSID. Warning: there is no warning before this happens. You click it, it's happening. Not that it is very hard to set back to whatever you want anyway.

# Accessing
Once the form is successfully submitted, you can change back to your home network (the ESP8266 will kick you off anyway) and you should see the newly created access point in your network settings. There are now two ways in which you can access it again. The first is to connect to the new network SSID (with password this time) and navigate back to 192.168.4.1 so you can see the form again. The second way is to stay on your home network and (if your devices support mDNS) connect to it using a special local network name. This name is whatever you named the device minus the spaces. i.e. *Living Room Light* becomes *livingroomlight.local* on the network. This is another reason to not get crazy with the names (not to mention that Alexa may have issue with it as well).

Now if you ever have to move the device to a different location, you need only connect to it and change it through the form.
(**Side note:** since this creates a unique id that Alexa stores, when you change the name you may not have to have Alexa poll for new devices again. She will randomly check on devices and will automatically update the name if you changed it.)

**Disclaimer:**
I am in no way an expert on the ESP8266, development boards or micro-electronics in general. However, I have started to experiment extensively with them. I am a mobile application developer professionally which has led me to working with many peripherals including Bluetooth LE scanners and dongles, card reader sleds, and others. I have now ventured in to the world of IoT and I'm having fun so...

\* Sorry, but I didn't capture any names as I was furiously typing away at this and it didn't originally occur to me to give credit. On the other hand, though, these were all open source with license to change the code for any purpose. But just to be clear, I'm feel a little bad about not giving the correct credits. As retribution, feel free to leave out my name if your next works uses code from here. :)
