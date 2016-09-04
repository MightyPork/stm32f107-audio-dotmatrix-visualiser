# STM32F107 Audio Visualiser

Simple audio visualiser with a dot matrix LED display.

[Video](https://twitter.com/MightyPork/status/772210972228456448)

## Functions

- Using the central joystick button, select render mode.
- Arrows left, right adjust brightness.
- Arrows up, down adjust sensitivity.

Modes include:
 
- waveform display with simple triggering
- FFT display
- Spindle FFT display (mirror effect)

## Target hardware

Developed for the "STEVAL-PCC012V1" evaluation board with STM32F107, paired with a "TS4657 Audio card" daughter board.

The skeleton of the project is generated using *STM32CubeMX*, with some minor modifications. The majority of the visualiser code is localed in `User\user_main.c`. The other files in `User/` are helper functions and modules. It's a CLion project, using CMake.

The project uses USART1 for debug messages (`PB6` - Tx, `PB7` - Rx). The display is controlled by multiple daisy-chained MAX2719 drivers (available on ebay as modules). The first driver is interfaced over SPI1 (`PA5` - SCK, `PA7` - MOSI, `PE6` - CS)

There is a 5-direction joystick connected to `PE10`, `PE11`, `PE12`, `PE13`, `PE14`.

The Audio daughter board carries a ST472IQT microphone pre-amt with an electret microphone.

For details, see documents *UM0896* and *UM0722*.

## Porting

The project will work without bigger changes on any STM32Fx, you just have to adjust the pin mapping and update the linker script and defines. That can be done with some attention using *STM32CubeMX*.

Note that some settings that can't be adjusted in CubeMX directly are changed in this project. Use git diff to see what changed after you re-generate the initialization files, and revert those changes. They are marked by comments.

## TODO

- Save preferences to Flash
