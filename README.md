# solar-blinds
Smart Venetian Blinds

## The really big problem
Should the bedroom blinds be open or closed when you go to bed?

If you leave them open the sun's warming rays will gently wake you from slumber, but nosey neighbors can peek into your bedroom. If you close them in the evening you're safe from prying eyes, but you'll have one of these awful mornings where you feel like sleeping another 4 hours. It's a dilemma.

Luckily, there is a solution.

## Solar blinds
Upgrade your blinds with a servo motor controlled by a Wifi-enabled microcontroller. After getting the current time of day via NTP it will calculate the time of sunrise and sunset. The blinds fade to closed after sunset and slowly open just before sunrise. There is also a manual override via the Blynk-powered app and Google/Alexa voice control.

## Ingredients
Hardware:
 * [NodeMCU](https://en.wikipedia.org/wiki/NodeMCU) devkit
 * Servo motor
 * Shaft coupling

Software:
 * [NTPClient](https://github.com/arduino-libraries/NTPClient) to get current time via Internet
 * [SunMoon](https://github.com/sfrwmaker/sunMoon) to calculate sunrise/sunset for given location
 * [Blynk](https://github.com/blynkkk/blynk-library) for app and [HTTP Api](https://blynkapi.docs.apiary.io/#)
 * [Timezone](https://github.com/JChristensen/Timezone) to calculate UTC for open/close times given via app
 * [IFTTT](https://community.blynk.cc/t/how-to-integrate-blynk-and-ifttt-google-assistant/16107) for voice control
