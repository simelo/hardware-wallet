# Skycoin electronic wallet firmware

This firmware had been copied and modified from [trezor-mcu](https://github.com/trezor/trezor-mcu) under the GNU LGPL. Please refer to the [README.md file](https://github.com/trezor/trezor-mcu/blob/master/README.md) on that repository for extra details about bootloader and firmware compilation.

This code aims at tranforming the cipher library from [this repository](https://github.com/skycoin/skycoin/tree/develop/src/cipher) into firmware for the STM32 hardware.

## 0. Windows setup

If you want to build project on Windows, you should install following tools:

### MSYS2

The reason we use MSYS2:
1) It supports SDL2 (Cygwin throw error "Unable to initialize SDL: no video device")
2) It supports POSIX system calls (no support in MINGW)

To install MSYS2, go to [MSYS2 homepage](https://www.msys2.org), choose the proper installer, download it and follow installation instructions on the homepage

### Packages

After MSYS2 is installed, the following packages should be installed:

1) Make
2) GCC
3) Unzip
4) Pip3

```
pacman -Sy make gcc unzip python-pip
```

Libraries installations are described below

### Warnings

1) Somehow MSYS2 doesn't remember changes of PATH variable. So, you need to add binaries of arm-none-eabi-gcc.

2) There is no SLD2 package, which is officially supported by MSYS2. But there is one for MINGW32/64. We used exactly this one. To make it work,
   after package installation (described in 7), you need to go /c/path_to_msys64/msys64/mingw64 or /c/path_to_msys64/msys64/mingw32 directly in MSYS2
   and copy:
   1) whole SDL2 folder from mingw../inlude to /usr/include
   2) SDL2.dll and sdl2-config from mingw../bin to /usr/binary
   3) libSDL2.a, libSDL2.dll.a, libSDL2main.a, libSDL2_text.a from mingw../lib to /usr/lib

3) If you use emulator, after first build and after each "make clean" command - run prepare_emulator.py script


## 1. Prepare environment

### Download and install GNU ARM Embedded Toolchain

On GNU/Linux it is possible to download the official compressed tarball, extract it , and add it to system `PATH` as follows

```sh
    wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/6-2017q2/gcc-arm-none-eabi-6-2017-q2-update-linux.tar.bz2
    tar xjf gcc-arm-none-eabi-6-2017-q2-update-linux.tar.bz2
    export PATH="$PWD/gcc-arm-none-eabi-6-2017-q2-update/bin:$PATH"
```

Some distributions offer binary installation packages . For instance, on Ubuntu>=14.04

```sh
sudo apt-get install gcc-arm-none-eabi
```

On Mac OS, after [installing homebrew](https://docs.brew.sh/Installation) it is recommended to install the toolchain like this

```sh
brew tap skycoin/homebrew-skycoin
brew update
brew install gcc-arm-none-eabi-63
```

On Windows, go to https://developer.arm.com and download [toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads), depending of your Windows version. Install it (don't forget to add binaries). After MSYS2 setup, add binaries to PATH variable:

```sh
export PATH=$PATH:/c/path_to_binaries/bin
```


### Install ST-LINK

[ST-LINK](https://github.com/texane/stlink) tool is needed to send JTAG commands to the Skycoin hardware wallet.
Binaries are available for [installing ST-LINK using native package managers](https://github.com/texane/stlink#installation).
For instance, on Mac OS X the following command will install `stlink` and its dependencies.

```
brew install stlink
```

On Windows, the best way to flash Skywallet is ST-Link Utility - can be downloaded [here](https://www.st.com/en/development-tools/stsw-link004.html)
WARNING: Due to GUI interface make st-flash command won't work!!!

If this option is not available for the platform of your preference then install from sources
by following the steps [here](https://github.com/texane/stlink/blob/master/doc/compiling.md).

### Install google protobuf

On Debian and Ubuntu GNU/Linux

```
sudo apt-get install protobuf-compiler python-protobuf golang-goprotobuf-dev
```

On Mac OS

```
brew install protobuf
go get -u github.com/golang/protobuf/{proto,protoc-gen-go}
```

On Windows (MSYS2)

```
pacman -S protobuf
```

If no binaries are available for your platform then

- [Install Protobuf C++ runtime and protoc](https://github.com/protocolbuffers/protobuf/tree/master/src) from sources.
- [Install Protobuf Python runtime](https://github.com/protocolbuffers/protobuf/tree/master/python#installation) from sources.
- [Install Protobuf Go runtime](https://github.com/golang/protobuf#installation) from sources.

### Configure your usb module

We need to tell your kernel to use the [hidraw module](https://www.kernel.org/doc/Documentation/hid/hidraw.txt) to communicate with the hardware device. If you don't your kernel will treat the device as a mouse or a keyboard.

#### Automatically

Run `sudo ./prepare-sky-hw.sh`.

#### Manually

Create a file named 99-dev-kit.rules in your /etc/udev/rules.d/ folder and write this content in that file (*super user priviledges are required for this step*).

    ## 0483:df11 STMicroelectronics STM Device in DFU Mode
    SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="df11", MODE="0666"
    ## 0483:3748 STMicroelectronics ST-LINK/V2
    SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="3748", MODE="0666"
    ## 0483:374b STMicroelectronics ST-LINK/V2.1 (Nucleo-F103RB)
    SUBSYSTEM=="usb", ATTR{idVendor}=="0483", ATTR{idProduct}=="374b", MODE="0666"
    ## 313a:0001
    SUBSYSTEM=="usb", ATTR{idVendor}=="313a", ATTR{idProduct}=="0001", MODE="0666"
    KERNEL=="hidraw*", ATTRS{idVendor}=="313a", ATTRS{idProduct}=="0001", MODE="0666"

Restart your machine or force your udev kernel module to [reload the rules](https://unix.stackexchange.com/questions/39370/how-to-reload-udev-rules-without-reboot).

    sudo udevadm control --reload-rules
    sudo udevadm trigger

## 2. Extra options while building the firmware

[Optional] If you want to compile with a inverted screen and inverted buttons set REVERSE_SCREEN and REVERSE_BUTTONS environment variable to 1 :

    export REVERSE_SCREEN=1
    export REVERSE_BUTTONS=1 # need to be changed in the project Makefile if you are using it

Disable the signature checking

Change SIGNATURE_PROTECT to 0 in the [project Makfile](https://github.com/skycoin/hardware-wallet/blob/master/Makefile)

## 3. How to burn the firmware in the device

### Use ST-LINK to burn the device

You can check the device is seen by your ST-LINK using this command:

    st-info --probe

To flash a full firmware build at `FULL_FIRMWARE_PATH` on a microcontroller of STM32f2xx family the command is:

    st-flash write ${FULL_FIRMWARE_PATH} 0x08000000;

It is also possible to use the `st-flash` rule in the [main Makefile](https://github.com/skycoin/hardware-wallet/blob/master/Makefile). If `FULL_FIRMWARE_PATH` not set it's value defaults to `./full-firmware-no-mem-protect.bin`, which happens to be the output of `make full-firmware` command.

On Windows, open ST-Link Utility. Choose "File" -> "Open File", and choose proper .bin file. Then connect ST-Link to your computer, choose "Target" -> Connect.
When ST-Link is successfully connected (see in console), choose "Target" -> "Erase". Wait, until previous firmware will erased. Then choose "Target" -> "Flash" and tap
"Start" button.  

## 4. Firmware signature

### Skip the wrong firmware signature warning

We are compiling a modified version of trezor original firmware. The bootloader we are using is configured to detect it and warn the user about this.
We are still in developpement steps, this warning is acceptable.
The devices allows the user to skip the warning but during that time the OS of the host computer may miss the opportunity to load the driver to communicate with the device.

If when you plug the device your OS is not seeing the device, skip the warning on device's screen saying that the signature is wrong and then try [this](https://askubuntu.com/questions/645/how-do-you-reset-a-usb-device-from-the-command-line).

If you are fast enough you can also quickly click the button "accept" on the device when the warning about wrong firmware signature appears.

#### You need three signatures:

The system stores five public keys and expects three signatures issued from one of these public keys.

The public keys are hardwritten in the bootloader's source code in file [signatures.c](https://github.com/skycoin/hardware-wallet/blob/master/tiny-firmware/bootloader/signatures.c)

The signatures are also present in [firmware_sign.py](https://github.com/skycoin/hardware-wallet/blob/master/tiny-firmware/bootloader/firmware_sign.py) script, in the "pubkeys" array.

#### Use your secret key to perform signature

See [README.md](https://github.com/skycoin/hardware-wallet/blob/master/README.md): Run `make sign` from repository home.

The command line tool will ask you in which of the three slots do you want to store the signature.

The it will ask you to provide a secret key that must correspond to one of the five public keys stored in the bootloader and the script as described above.

#### Recombine the firmware and the bootloader

See [README.md](https://github.com/skycoin/hardware-wallet/blob/master/README.md): Run `make full-firmware` from repository home.

Then you can re-flash the firmware for instance with st-skycoin alias.

## 5. Communicate with the device

### Use golang code examples

Check [here](https://github.com/skycoin/hardware-wallet-go/) for golang code example communicating with the device.

Feel free to hack [main.go](https://github.com/skycoin/hardware-wallet-go/blob/master/main.go) file.

You can also try the trezorctl [python based command line](https://github.com/trezor/python-trezor).

## 6. How to read the firmware's code

The communication between PC and firmware is a master/slave model where the firmware is slave.
It reacts to messages but cannot initiate a communication.
The messages are defined using google protobuf code generation tools. The same file messages.proto can be copy pasted elswhere to generate the same structures in other coding languages.

The [repository](https://github.com/skycoin/hardware-wallet-go/) provides examples to communicate with the device using golang.

The firmware has two components: the [bootloader](https://github.com/skycoin/hardware-wallet/tree/master/tiny-firmware/bootloader) and the [firmware](https://github.com/skycoin/hardware-wallet/tree/master/tiny-firmware/firmware).
The bootloader main role is to check firmware's signature in order to warn user in case the detected firmware is not the official firmware distributed by skycoin.

Here is a quick presentation of most important files of the firmware:

* protob/messages.proto: defines all the messages received the firmware can receive and their structure
* firmware/fsm.c: all the messages received in the firmware correspond to a call to a function in fsm.c, the corresponding between messages and function is defined in protob/messages_map.h (generated file)
* firmware/storage.c: manages the persistent (persistent when pluging out the power supply) memory of the firmware.
* oled.c/layout.c: manage screen display
* firmware/skywallet.c: main entry point

On bootloader side it is worth mentioning:

* signatures.c: checks the firmware's signature matches skycoin's public keys

## 7. Troobleshooting
If you get SDL errors you might want to install these:

    sudo apt-get install libsdl2-dev libsdl2-image-dev

If you can't install sdl packages using 'apt install' on an ubuntu based linux distribution (had the problem on linux mint) you can download them from these urls:

* https://packages.ubuntu.com/bionic-updates/libsdl2-dev
* https://packages.ubuntu.com/bionic/libsdl2-2.0-0

Works also with docker if you run the script:

    ./build-emulator.sh

On Windows (MSYS2)

```
#For x86
pacman -S mingw-w64-i686-SDL2

#For x86_64
pacman -S mingw-w64-x86_64-SDL2
```
