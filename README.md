PiClock
=======

Simple studio clock for linux. This was originally built using OpenVG on
Raspberry Pi for its output, but now uses plain OpenGL and works fine on
normal XOrg on Debian.

Checkout
--------

Checkout this project by:

```shell
        # Install git if you don't already have it
	sudo apt-get install git
        # Checkout the main project and it's submodules
	git clone --recursive https://github.com/simonhyde/PiClock.git
```

Build
-----

1. First you'll need to install some dependencies (ntpdate is only suggested for runtime):

```shell
	sudo apt-get install libjpeg-dev libboost-program-options-dev libboost-system-dev libssl-dev libmagick++-dev libb64-dev freeglut3-dev libglew-dev x11-xserver-utils xinit ntp ntpdate libglfw3-dev premake4
```

2. Change to the directory you checked the code out into; probably:
	
```shell
	cd PiClock
```

3. Compile:
	
```shell
	make
```

4. Run:

```shell
	./piclock
```


Configure NTP
--------------------

You may want to add/change your NTP servers (in /etc/ntp.conf)


Running at startup
------------------

To configure this to run at startup, I did the following:

1. Add a new user to run the clock:

```shell
	sudo adduser --disabled-password piclock
	sudo usermod --append --groups spi,video piclock
```

2. Make the user profile run the clock:

```shell
	sudo editor ~piclock/.bashrc

	# And add a line to the end, something like: /home/pi/PiClock/piclock-startx-wrapper
```

3. Enable text-mode autologin using raspi-config:

```shell
	sudo raspi-config
	
	#Boot Options, Desktop/CLI, Console Autologin
```

4. Make the system auto-login as the piclock user:

```shell
	sudo editor /etc/systemd/system/getty@tty1.service.d/autologin.config
```

  and change:

```
	ExecStart=-/sbin/agetty --autologin pi --noclear %I $TERM
```

  to:

```
	ExecStart=-/sbin/agetty --autologin piclock --noclear %I $TERM
```
  (ie change pi to piclock)

Switching to Read Only SD Card
------------------------------

Once you've got everything working, you may want to make the SD card read-only, to prevent future corruption/wearing out the SD card. The easiest way to do this nowadays is to use the Overlay FS option built into raspi-config, however this seems to cause the network interface to be accidentally renamed, so you first have to delete the rule that's messing that up:

```shell
	sudo rm /lib/udev/rules.d/73-usb-net-by-mac.rules

	sudo raspi-config
	#Advanced, Overlay FS, Enable Overlay FS, and set boot filesystem to write-protected/read-only
```
