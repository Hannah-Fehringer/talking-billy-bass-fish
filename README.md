# talking-billy-bass-fish
A Bluetooth‑enabled, voice‑reactive, animatronic Billy Bass

This project turns the iconic Billy Bass fish into a fun, interactive audio interface.
It connects via Bluetooth and reacts to sound, making it usable as a playful front‑end for a voice assistant.
The Arduino firmware animates the head, mouth, and tail (boredom flap) based on audio envelope detection.



## ⚙️ Installation

1. Clone the repo:
```
git clone https://github.com/Hannah-Fehringer/talking-billy-bass-fish.git
```

2. Open the sketch:
```arduino/BillyBass/BillyBass.ino```

3. Install required libraries .
4. Choose your board + COM port.
5. Upload and enjoy your very talkative fish.

## 🎤 Inspiration & Credits
This project is based on:

BTBillyBass by TensorFlux
- 👉 https://github.com/TensorFlux/BTBillyBass

Hardware ideas adapted from:
- 👉 https://maker.pro/arduino/projects/how-to-animate-billy-bass-with-bluetooth-audio-source

However, I did not use the potentiometer shown in the guide.
Instead, I built a voltage divider using two 100 kΩ resistors to raise the audio signal into a usable range for the Arduino’s ADC.

## 🧠 Features

- 🎧 Bluetooth audio input
- 🔊 Audio envelope detection for real-time movement
- 🐟 Animated mouth + head + body
- 💤 “Boredom mode”: tail flaps when quiet for too long
- 🎛️ Cleaner signal conditioning using a 100 kΩ + 100 kΩ voltage divider
- 💡 Fully non‑blocking animation logic
- 🛠️ Compatible with Arduino Nano / Uno / ESP boards

## 🐠 How It Works
1. Audio Processing

- The audio signal enters through the voltage divider
- Arduino reads it using analogRead()
- A smoothed “envelope” value determines fish activity

2. Animation Logic

- Mouth opens when audio amplitude rises
- Head moves forward when speaking
- When quiet:
  - After X seconds → boredom logic triggers
  - Tail flaps randomly every few seconds

## 📜 License
MIT — free to use, modify, improve.
