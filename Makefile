#simple root level makefile by Father Nitwit

all:
	$(MAKE) -C EMuShareMem
	$(MAKE) -C world
	$(MAKE) -C zone
	$(MAKE) -C eqlaunch
	$(MAKE) -C mailserver
	$(MAKE) -C chatserver
	$(MAKE) -C ucs
	$(MAKE) -C utils

clean:
	$(MAKE) -C EMuShareMem clean
	$(MAKE) -C world clean
	$(MAKE) -C zone clean
	$(MAKE) -C eqlaunch clean
	$(MAKE) -C mailserver clean
	$(MAKE) -C chatserver clean
	$(MAKE) -C ucs clean
	$(MAKE) -C utils clean

