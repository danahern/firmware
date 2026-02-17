# Build all A7 Linux apps for STM32MP1
#
# Run inside the Buildroot container after build completes:
#   linux-build.build(container, command="make -C /workspace/firmware/linux/apps all install")

APPS = hello rpmsg_echo

.PHONY: all clean install $(APPS)

all: $(APPS)

$(APPS):
	$(MAKE) -C $@

install:
	@for app in $(APPS); do \
		$(MAKE) -C $$app install; \
	done

clean:
	@for app in $(APPS); do \
		$(MAKE) -C $$app clean; \
	done
