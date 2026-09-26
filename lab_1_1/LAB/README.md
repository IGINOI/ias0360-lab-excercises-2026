# Modes application

This is the Lab 1.1 application scaffold. It reuses the supplied LCD,
touchscreen, SD-card, and IMU drivers from the neighboring example projects.

The intended mode cycle is:

```text
Idle -> IMU -> Drawing -> Idle
```

One press of the Eval Board `KEY` button should advance to the next mode.
Verify the button GPIO from the Pico-Eval Board schematic before flashing and
replace `KEY_GPIO` and `LED_GPIO` in `main.cpp` if necessary. The scaffold uses
GPIO 0 for `KEY` and GPIO 25 for the LED only as compile-time placeholders.

Build from this directory inside the development container:

```text
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

The generated firmware will be `build/modes.uf2`.

The application is intentionally incomplete. The next steps are button
debouncing, SD-card mounting, IMU buffering, LCD display, drawing storage, and
reading files back with FatFs `f_open` and `f_read`.