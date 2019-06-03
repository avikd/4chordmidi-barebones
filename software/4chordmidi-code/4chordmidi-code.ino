/*
 * 4chord MIDI barebones design, running on an Arduino Pro Micro
 * 
 * Goal was to develop a very simple design that can be 
 * prototyped on a breadboard, or soldered together on a 
 * perfBoard with minimal components, and minimal code to
 * get the device up and running, allowing flexibility
 * for the user to hack together more features.
 * 
 * Serves as a good introduction to Arduino and MIDI devices. 
 * 
 * Inspired from the original 4Chord-MIDI by Sven Gregori,
 * source for which can be found at [ https://github.com/sgreg/4chord-midi ]
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/
 *
 */


 /*
  * Libraries necessary to compile the code:
  * 1. MIDIUSB [ https://github.com/arduino-libraries/MIDIUSB ]
  * 2. Adafruit-GFX-Library [ https://github.com/adafruit/Adafruit-GFX-Library ]
  * 3. Adafruit PCD8544 [ https://github.com/adafruit/Adafruit-PCD8544-Nokia-5110-LCD-library ]
  * 4. AceButton [ https://github.com/bxparks/AceButton ]
  * 
  * Suggested way of installing the above libraries is thorugh
  * the Library Manager provided in Arduino.
  */
 
#include "MIDIUSB.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <AceButton.h>
using namespace ace_button;

#define PLAYBACK_KEY_MAX 12

int currentKey = 0;


// Button connections to Arduino pin number
#define btn1 3
#define btn2 4
#define btn3 5
#define btn4 6

#define btn_up 20
#define btn_down 18

// We use the maximum velocity for every key pressed, since 
// our buttons are not velocity sensitive
#define velocity_ 127

AceButton button1;
AceButton button2;
AceButton button3;
AceButton button4;

ButtonConfig uiConfig;
AceButton button_up(&uiConfig);
AceButton button_down(&uiConfig);


void handleChordButtons(AceButton*, uint8_t, uint8_t);
void handleUiButtons(AceButton*, uint8_t, uint8_t);

int chordPressed = 0;
int uiPressed = 0;

typedef struct {
  /* root note */
  uint8_t root;
  /* major (in chord I, V and IV) or minor (in chord vi) third */
  uint8_t third;
  /* perfect fifth */
  uint8_t fifth;
  /* octave */
  uint8_t octave;
} chord_t;

static chord_t chord;

static const uint8_t root_notes[PLAYBACK_KEY_MAX][4] = {
  {48, 43, 45, 41},   /* C3   G2  A2  F2  */
  {49, 44, 46, 42},   /* C#3  G#2 Bb2 F#2 */
  {50, 45, 47, 43},   /* D3   A2  B2  G2  */
  {51, 46, 48, 44},   /* D#3  Bb2 C3  G#2 */
  {52, 47, 49, 45},   /* E3   B2  C#3 A2  */
  {53, 48, 50, 46},   /* F3   C3  D3  Bb2 */
  {54, 49, 51, 47},   /* F#3  C#3 D#3 B2  */
  {43, 50, 52, 48},   /* G2   D3  E3  C3  */
  {44, 51, 53, 49},   /* G#2  D#3 F3  C#3 */
  {45, 52, 54, 50},   /* A2   E3  F#3 D3  */
  {46, 41, 43, 39},   /* Bb2  F2  G2  D#2 */
  {47, 42, 44, 40}    /* B2   F#2 G#2 E2  */
};

/* list of offsets to the chord's major (I, V, IV) or minor (vi) third */
static const uint8_t third_offset[] = {4, 4, 3, 4};

/* offset to the chord's perfect fifth, same for all */
static const uint8_t fifth_offset = 7;

/* offset to the root octave */
static const uint8_t octave_offset = 12;
void construct_chord(uint8_t);

// Declare LCD object for software SPI
// Adafruit_PCD8544(CLK,DIN,D/C,CE,RST);
Adafruit_PCD8544 display = Adafruit_PCD8544(16, 15, 9, 8, 7);

int i, Cx, Cy = 0;

char *notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

void setup() {
  // put your setup code here, to run once:
  
  // Initialize the LCD 
  initLcd();


  // configure Event Handler for Chord Button press
  ButtonConfig* chordButtons = ButtonConfig::getSystemButtonConfig();
  chordButtons->setEventHandler(handleChordButtons);

  // configure Event Handler for UI Button press
  //  ButtonConfig* uiButtons = ButtonConfig::getSystemButtonConfig();
  uiConfig.setEventHandler(handleUiButtons);

  // Show Splash screen
  splashScreen();

  // initialize all Button Inputs with Internal Pullup
  pinMode(btn1, INPUT_PULLUP);
  pinMode(btn2, INPUT_PULLUP);
  pinMode(btn3, INPUT_PULLUP);
  pinMode(btn4, INPUT_PULLUP);
  pinMode(btn_up, INPUT_PULLUP);
  pinMode(btn_down, INPUT_PULLUP);

  // initialize chord buttons
  button1.init(btn1, HIGH, 0);
  button2.init(btn2, HIGH, 1);
  button3.init(btn3, HIGH, 2);
  button4.init(btn4, HIGH, 3);

  // initialize UI/up-down buttons. Select button ins't implemented yet
  button_up.init(btn_up, HIGH, 4);
  button_down.init(btn_down, HIGH, 5);

}

void loop() {
  // put your main code here, to run repeatedly:
  // Just poll the buttons here
  pollButtons();
}

// Function that checks for the last Musical Key the device is in,
// and changes the key according to the buttons pressed
void updateKey(int buttonPressed) {
  if (buttonPressed == 4)
  {
    currentKey += 1;
    if (currentKey > 11) {
      currentKey = 0;
    }
  }

  if (buttonPressed == 5)
  {
    currentKey -= 1;
    if (currentKey < 0)
    {
      currentKey = 11;
    }
  }

  if (currentKey == 1 or
      currentKey == 3 or
      currentKey == 6 or
      currentKey == 8 or
      currentKey == 10)
  {
    Cx = 20;
    Cy = 10;
  }
  else {
    Cx = 30;
    Cy = 10;
  }


  display.clearDisplay();
  display.setTextSize(4);
  display.setTextColor(BLACK);
  display.setCursor(Cx, Cy);
  display.drawRect(0, 0, 84, 48, 1);
  display.drawRect(1, 1, 82, 46, 1);
  display.println(notes[currentKey]);
  display.display();
  display.clearDisplay();
}


// Function to initialse the LCD display
void initLcd() {
  display.begin();
  display.setContrast(57);
  display.clearDisplay();
  // Display Text
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(32,6);
  display.println("PiyaNo!");
  display.display();
  delay(100);
  display.clearDisplay();
}


// Callback function that handles Chord button presses and releases
// In this case, it just constructs the chords depending on the
// note that is pressed, and sends the root, third & fifth over MIDI
void handleChordButtons(AceButton*  button , uint8_t eventType,
                        uint8_t /* buttonState */) {
  switch (eventType) {

    case AceButton::kEventPressed:
      chordPressed = button->getId();
      construct_chord(chordPressed);
      sendMidiOn(chord.root);
      sendMidiOn(chord.third);
      sendMidiOn(chord.fifth);
      break;

    case AceButton::kEventReleased:
      chordPressed = button->getId();
      construct_chord(chordPressed);
      sendMidiOff(chord.root);
      sendMidiOff(chord.third);
      sendMidiOff(chord.fifth);
      break;
  }
}

// Callback function that handles the UI button presses
// In this case, updateKey is called, and that changes the musical key you're playing in 
void handleUiButtons(AceButton*  button2 , uint8_t eventType,
                     uint8_t /* buttonState */) {
  switch (eventType) {

    case AceButton::kEventPressed:
      uiPressed = button2->getId();
      updateKey(uiPressed);
      break;
  }
}


// Function to construct the chords, with input being the chord number
void construct_chord(uint8_t chord_num)
{
  uint8_t key = currentKey;

  chord.root   = root_notes[key][chord_num];
  chord.third  = chord.root + third_offset[chord_num];
  chord.fifth  = chord.root + fifth_offset;
  chord.octave = chord.root + octave_offset;
}

// Function to send MIDI Note On data, with a fixed velocity
void sendMidiOn(int note_) {
  noteOn (0, note_, velocity_);
  MidiUSB.flush();
  //Serial.println(btn);
}

// Function to send MIDI Note Off data, with a fixed velocity
void sendMidiOff(int note_) {
  noteOff (0, note_, velocity_);
  MidiUSB.flush();
  //Serial.println(btn);
}

// Function to compose the On MIDI packet and actually write it to hardware
void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOn);
}

// Function to compose the Off MIDI packet and actually write it to hardware
void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}


// Button polling function -- Checks for any buttons being pressed
// If a button press is detected, it is handled internally by the Acebutton Lib
void pollButtons() {
  button1.check();
  button2.check();
  button3.check();
  button4.check();
  button_up.check();
  button_down.check();
}


// Function to draw the Splash Screen at boot time for 2 seconds
void splashScreen()
{
    display.clearDisplay();
    display.setCursor(0,0);
    display.drawRect(7,6,11,24,1);
    display.drawRect(17,6,11,24,1);
    display.drawRect(27,6,11,24,1);
    display.drawRect(37,6,11,24,1);
    display.drawRect(47,6,11,24,1);
    display.drawRect(57,6,11,24,1);    
    display.drawRect(67,6,11,24,1);
    display.fillRect(15,6,5,12,1);
    display.fillRect(25,6,5,12,1);
    display.fillRect(35,6,5,12,1);
    display.fillRect(55,6,5,12,1);
    display.fillRect(65,6,5,12,1);
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.setCursor(8,32);
    display.println("4Chord MIDI!");
    display.display();
    delay(100);
    display.clearDisplay();
 }
