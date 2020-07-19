.DEFAULT_GOAL = all

.PHONY = all install uninstall clean

all: make_host_manager_11ad make_shell_11ad make_wiburn make_wlctutils make_wlctpciacss make_flashacss

export CROSS
export WIGIG_3PP_BUILD

make_host_manager_11ad:
	$(MAKE) -C host_manager_11ad

make_shell_11ad:
	$(MAKE) -C shell_11ad

make_wiburn: make_wlctutils make_wlctpciacss make_flashacss
	$(MAKE) -C wiburn

make_wlctutils:
	$(MAKE) -C lib/utils

make_wlctpciacss:
	$(MAKE) -C lib/WlctPciAcss

make_flashacss:
	$(MAKE) -C lib/FlashAcss

clean:
	echo "Cleaning up wigig debug tools"
	$(MAKE) -C host_manager_11ad clean
	$(MAKE) -C shell_11ad clean
	$(MAKE) -C wiburn clean
	$(MAKE) -C lib/utils clean
	$(MAKE) -C lib/WlctPciAcss clean
	$(MAKE) -C lib/FlashAcss clean

install: all

	@echo "Installing wigig debug tools libraries to /usr/sbin"
	install lib/utils/libwigig_utils.so /usr/lib
	install lib/FlashAcss/libwigig_flashaccess.so /usr/lib
	install lib/WlctPciAcss/libwigig_pciaccess.so /usr/lib

	@echo "Installing wigig debug tools to /usr/sbin"
	install host_manager_11ad/host_manager_11ad /usr/sbin
	install shell_11ad/shell_11ad /usr/sbin
	install wiburn/wigig_wiburn /usr/sbin

uninstall:

	rm -f /usr/sbin/host_manager_11ad
	rm -f /usr/sbin/shell_11ad
	rm -f /usr/sbin/wigig_*
	rm -f /etc/wigig_*
	rm -f /usr/lib/libwigig_*

