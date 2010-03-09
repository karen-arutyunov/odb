GXX=g++-4.5
PLUGIN_INC := $(shell $(GXX) -print-file-name=plugin)

src := plugin.cxx
obj := $(src:.cxx=.o)

odb.so: $(obj)
	$(GXX) -shared -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS)

%.o: %.cxx
	$(GXX) -c -o $@ -fPIC $(CPPFLAGS) -I$(PLUGIN_INC)/include $(CXXFLAGS) $<

# Test.
#
.PHONY: test
test: odb.so test.cxx
	$(GXX) -x c++ -S -fplugin=./odb.so test.cxx

# Clean.
#
.PHONY: clean
clean:
	rm -f *.o odb.so
