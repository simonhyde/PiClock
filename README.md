PiClock
=======

Simple Clock for the Raspberry Pi, using OpenVG for its output

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
	sudo apt-get install libjpeg-dev ntpdate ttf-dejavu libboost-program-options-dev libboost-system-dev libssl-dev libmagick++-dev libb64-dev
	```

2. Compile:
	
	```shell
	make
	```

3. Run:

	```shell
	./piclock
	```

Clock Unsynchronised
--------------------

When ntp notices a large jump in time (such as when first booting up the Raspberry Pi), it tells clients it is not synchronised for quite a while (many minutes). To work around this, I run ntpdate to manually crash in the time whenever a new network connection is started up, closing down and then re-starting the main NTP daemon whilst this happens. To do this I have a slightly modified ntpdate script in if-up.d, you can install this update script by:

1. Make sure ntpdate is installed:

	```shell
	sudo apt-get install ntpdate
	```

2. Copy in new script:
	
	```shell
	sudo cp if-up.d-ntpdate /etc/network/if-up.d/ntpdate
	```

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

	# And add a line to the end, something like: /home/pi/PiClock/piclock
	```

3. Make the system auto-login as the piclock user:

	```shell
	sudo editor /etc/systemd/system/autologin@.service
	```

  and change:

  	```
	ExecStart=-/sbin/agetty --autologin pi --noclear %I $TERM
	```

  to:

	```
	ExecStart=-/sbin/agetty --autologin piclock --noclear %I $TERM
	```

Switching to Read Only SD Card
------------------------------

Once you've got everything working, you may want to make the SD card read-only, to prevent future corruption/wearing out the SD card. Instructions for this can be found at https://www.raspberrypi.org/forums/viewtopic.php?p=213440
