# qemu-run
Quickly launch [QEMU](https://www.qemu.org/) virtual machines from your terminal. Without scripts, or bloated UI programs.

# How to install: Unix-like systems
For doing this, simple clone this repo, and build it, you will need tools like [git](https://git-scm.com/), a [compiler](https://gcc.gnu.org/)  and [make](https://www.gnu.org/software/make/).

	git clone https://github.com/lucie-cupcakes/qemu-run.git
	cd qemu-run
	make
	sudo mv qemu-run.bin /usr/bin/qemu-run

# How to use: Making your first VM
First be sure to have [QEMU](https://www.qemu.org/)  installed, after that we need to setup a directory where you are gonna save all your virtual machines. This are gonna be stored into an [environment variable](https://en.wikipedia.org/wiki/Environment_variable).
For doing this we simple edit our [bashrc](https://duckduckgo.com/?q=What%20is%20bashrc) to add this directories:

	echo "export QEMURUN_VM_PATH=\"$HOME/VM\"" >> $HOME/.bashrc

You can also specify multiple directories, by separating them by ":" , for example: 

	echo "export QEMURUN_VM_PATH="$HOME/VM:/media/volume/VM" >> $HOME/.bashrc

For this simple example, let's stick with `$HOME/VM`
Now, grab yourself an ISO of any operative system you want to emulate.
For this example, we are gonna download one of the smallest GNU/Linux distros: [TinyCore](http://www.tinycorelinux.net/).

	cd $HOME/VM
	mkdir TinyCore
	cd TinyCore
	echo "sys=x32" > config # Initialize the VM config file.
	curl http://www.tinycorelinux.net/12.x/x86/release/TinyCore-current.iso > TinyCore.iso
	ln -s TinyCore.iso cdrom # This is how the VM detects the optical media attached to it.
	qemu-run TinyCore
   You should be see Tiny Core Linux booting up 😃
   
   If you need a hard drive (for example, you are installing a GNU/Linux like Debian)

	qemu-img create -f qcow2 disk 64G # Replace the size as you like.
	
  
# Warning: Beta software!
This piece of software is barely tested, don't use it on mission critical systems.

# Free (as in freedom) software
This software is subject to the GPLv3 or Later LICENSE, check LICENSE file for more information,
Or go visit [GNU Project - GPLv3 Page](https://www.gnu.org/licenses/gpl-3.0.html)
