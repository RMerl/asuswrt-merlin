include ../../../build.mak

lib_dir:
	@for token in `echo $(APP_LDFLAGS)`; do \
		echo $$token | grep L | sed 's/-L//'; \
	done

inc_dir:
	@for token in `echo $(APP_CFLAGS)`; do \
		echo $$token | grep I | sed 's/-I//'; \
	done

libs:
	@for token in `echo $(APP_LDLIBS)`; do \
		echo $$token | grep \\-l | sed 's/-l//'; \
	done

