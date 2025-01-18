# GameTable.ino

## Overview

`GameTable.ino` is an Arduino-based project designed to enhance tabletop gaming experiences. It uses a NeoPixel LED strip, an ESP12-E controller, and additional components to create dynamic lighting effects for a gaming table. The setup includes zones for players with customizable colors and effects controlled via a central interface.

## Features

- **Dynamic Zone Lighting:** Each player's zone on the table is illuminated with distinct colors.
- **Adjustable Zone Sizes:** Player zones can be customized to match the table's layout.
- **Web Interface Control:** A ESP12-E hosts a web-based interface for the game master to control colors and zones.
- **Visual Effects:** Includes animations and effects to enhance immersion during gameplay.

## Components

- **Arduino Controller:** Runs the `GameTable.ino` code to control the NeoPixel LEDs.
- **NeoPixel LED Strip:** 31 LEDs per meter, surrounding the table's edge.
- **ESP12-E:** Hosts the web interface for controlling the setup.
- **Other Electronics:** Includes power supply and connectors.

## Setup

1. **Hardware Setup:**
   - Attach the NeoPixel LED strip around the table's edge.
   - Connect the ESP12-E to the LED strip and power supply.
   - Pin 14 / D4


2. **Software Installation:**
   - Upload the `GameTable.ino` code to the ESP12-E using the Arduino IDE.
   - Configure the web interface on the ESP12-E to control zones and colors.

3. **Configuration:**
   - Adjust the code to define the number of LEDs per zone based on your table layout.
   - Set up the zones and colors via the web interface.

## How to Use

1. Power on the setup.
2. Access the web interface hosted on the ESP12-E.
3. Configure zones and colors for each player.
4. Start your game and enjoy the immersive lighting effects!

## Example Code Snippet

```cpp
#include <Adafruit_NeoPixel.h>
#define PIN 14
#define NUM_LEDS 150

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
}

void loop() {
  // Example: Set first 10 LEDs to red
  for(int i = 0; i < 10; i++) {
    strip.setPixelColor(i, strip.Color(255, 0, 0));
  }
  strip.show();
}
```

## Dependencies

- [Adafruit NeoPixel Library](https://github.com/adafruit/Adafruit_NeoPixel)

## Future Improvements

- Integration with voice commands for hands-free control.
- Adding preset animations and themes for different game types.
- Expanding the web interface with advanced configuration options.

## Contributing

Contributions are welcome! Feel free to fork the repository, make changes, and submit a pull request. Please ensure your code follows the existing style and includes appropriate comments.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

---

Enjoy your enhanced gaming experience with `GameTable.ino`! If you have any questions or feedback, feel free to open an issue or reach out.

