GXX=g++-4.5
PLUGIN_INC := $(shell $(GXX) -print-file-name=plugin)

src := plugin.cxx
obj := $(src:.cxx=.o)

sunrise.so: $(obj)
	$(GXX) -shared -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS)

%.o: %.cxx
	$(GXX) -c -o $@ -fPIC $(CPPFLAGS) -I$(PLUGIN_INC)/include $(CXXFLAGS) $<

# Test.
#
.PHONY: test
test: sunrise.so test.cxx
	$(GXX) -x c++ -S -fplugin=./sunrise.so test.cxx

# Clean.
#
.PHONY: clean
clean:
	rm -f *.o sunrise.so
