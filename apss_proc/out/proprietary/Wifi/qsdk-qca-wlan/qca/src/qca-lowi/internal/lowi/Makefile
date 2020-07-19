SUBDIRS=./lowi_server ./test/lowi_test
SHAREDLIBS=./lowi_client

LOWI_MAKE_OPTS=\
    LOCAL_PATH_UTILS=$(LOWI_SRC_PATH)/util/native/

all: lowi
lowi:
	for dir in $(SHAREDLIBS) ; do (cd $$dir && $(MAKE) all $(LOWI_MAKE_OPTS)) || exit 1; done
	for dir in $(SUBDIRS) ; do (cd $$dir && $(MAKE) all $(LOWI_MAKE_OPTS)) || exit 1; done

clean:
	for dir in $(SHAREDLIBS) ; do (cd $$dir && $(MAKE) clean $(LOWI_MAKE_OPTS)) || exit 1; done
	for dir in $(SUBDIRS) ; do (cd $$dir && $(MAKE) clean $(LOWI_MAKE_OPTS)) || exit 1; done
	rm -f *~

install:
	for dir in $(SUBDIRS) ; do (cd $$dir && $(MAKE) install $(LOWI_MAKE_OPTS)) || exit 1; done
