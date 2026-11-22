# Fast E-ink reader firmware

This is bare-metal firmware to control a UC8179 7.5" eink screen and turn it into an e-reader.

It uses a custom LUT to decrease the page turning time from ~5s to 0.5s, making the whole reading experience a lot smoother. It is designed to run on the 20 cent CH32V203, using a bare metal architecture and only 12KiB of flash.

The custom LUT also turns the black & white display to 4 level grayscale, thus allowing anti-aliasing support for the text.

![](https://icy.computer/eink.png)

### Structure:

I've divided the project into 3 parts:

- `src`: Main ch32v203 firmware. Embeds a generated bitmap font in a custom format. Default font is `OpenSans`.
- `font`: Program to encode a ttf font into a custom-format c-header bitmap. Run `./render > ../src/glyphs.h` to update font.
- `sdcard`: Writes `data.txt` into raw sdcard data. (`data.txt` not included due to copyright - put your favorite book)

### Usage

First clone the repo and init submodules (`git submodule update --init --recursive`).

To flash your ch32v203, go to `src` and run `make` with a Link-E attached (Or use the bootloader with USB-C). Now you can connect the pins according to `src/ports.h`. The default usage is with nanoCH32V203 or Comu.

The font is already generated, but you can edit the program and replace the font file to use any ttf you want. (Please use verified fonts. The ttf implementation is NOT hardened against injections).

To write the book data to an sdcard. Go to `sdcard`, write your file to `data.txt` and run `./flash.sh`.

