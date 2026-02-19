# Build all Linux apps
#
# Pass BOARD= to select target (see app Makefiles for options):
#   make                        # STM32MP1 (default)
#   make BOARD=alif-e7          # Alif E7

APPS = hello rpmsg_echo display_test

.PHONY: all clean install $(APPS)

all: $(APPS)

$(APPS):
	$(MAKE) -C $@ BOARD=$(BOARD)

install:
	@for app in $(APPS); do \
		$(MAKE) -C $$app install BOARD=$(BOARD); \
	done

clean:
	@for app in $(APPS); do \
		$(MAKE) -C $$app clean; \
	done
