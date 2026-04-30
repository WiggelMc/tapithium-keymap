# Tapithium keymap for the Glove80 keyboard

Based on [Matt Sturgeon's Keymap Build System](https://github.com/MattSturgeon/glove80-config).

The keyboard firmware can be built locally using `nix`, which is also used for [automatic builds].

[Keymap Drawer] is used to render images of each layer (see below).

## Layers

### Tapithium Mods

![Mods L](img/glove80_mods_l.svg)
![Mods R](img/glove80_mods_r.svg)

### Typing Layers

![Base](img/glove80_base.svg)
![Symbol](img/glove80_symbol.svg)
![Number](img/glove80_number.svg)
![Command](img/glove80_command.svg)
![Function](img/glove80_function.svg)
![Nav](img/glove80_nav.svg)
![Magic](img/glove80_magic.svg)

### Functional Layers

![Gaming](img/glove80_gaming.svg)
![Mouse](img/glove80_mouse.svg)
![Mouse](img/glove80_exit.svg)

### Moergo Layers

![Moergo Magic](img/glove80_moergo_magic.svg)
![Factory](img/glove80_factory_test.svg)

## Setup

### Flashing Firmware

Before flashing, ensure `udisks2` is setup and enabled so that the keyboard can mount its bootloader storage device.

- [`services.udisks2.enable`](https://nixos.org/manual/nixos/stable/options#opt-services.udisks2.enable) NixOS option
- `systemctl status udisks2.service`
- `udisksctl status`

#### Flashing automatically

The easiest way to flash the firmware is using `nix run`.

- `cd` into the project root
- Connect the **right** half (in bootloader mode)
- Run `nix run`
- Connect the **left** half (in bootloader mode)
- Run `nix run`
- Done!

#### Flashing manually

You can also manually copy the `uf2` firmware file to the keyboard.

The firmware can be built locally using `nix build` and will be written to `./result/glove80.uf2`.
Alternatively, the firmware can be downloaded from [GitHub Actions][automatic builds]'s artifacts.

Connect the keyboard half in bootloader mode, then copy the firmware file into the keyboard's USB storage.
The keyboard will unmount when it has finished flashing.

If you do not have `udisks2` enabled, for example on Arch systems, you can run `./flash_arch.sh` to automatically mount and flash both sides.
