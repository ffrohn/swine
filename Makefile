# PREFIX is environment variable, but if it is not set, then set default value
ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

install:
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 build/swine $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/lib/
	install -m 644 build/libswine.a $(DESTDIR)$(PREFIX)/lib/
	install -d $(DESTDIR)$(PREFIX)/include/swine/
	install -m 644 include/swine.h $(DESTDIR)$(PREFIX)/include/swine/
	install -m 644 include/config.h $(DESTDIR)$(PREFIX)/include/swine/
	install -m 644 include/lemma_kind.h $(DESTDIR)$(PREFIX)/include/swine/
	install -m 644 include/preproc_kind.h $(DESTDIR)$(PREFIX)/include/swine/

uninstall:
	rm build/libswine.a $(DESTDIR)$(PREFIX)/lib/libswine.a
	rm -r $(DESTDIR)$(PREFIX)/include/swine/
