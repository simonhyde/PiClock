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
	sudo apt-get install libjpeg-dev ntpdate
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
	sudo adduser piclock
	```

2. Make the user profile run the clock:

	```shell
	sudo editor ~piclock/.bashrc

	# And add a line to the end, something like: /home/pi/PiClock/piclock
	```

3. Make the system auto-login as the piclock user:

	```shell
	sudo editor /etc/inittab
	```

  and then change the following 2 lines:

	1:2345:respawn:/sbin/getty -a clock --noclear 38400 tty1 
	2:23:respawn:/sbin/getty 38400 tty2

  to:

	1:2345:respawn:/sbin/getty -a piclock --noclear 38400 tty1 
	2:2345:respawn:/sbin/getty 38400 tty2
