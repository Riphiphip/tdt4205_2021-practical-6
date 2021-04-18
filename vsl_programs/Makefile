TARGETS=$(shell ls *.vsl | sed s/\.vsl//g)
all: ${TARGETS}

# Currently the binaries won't compile directly
# to do so remove the "\.s" from the TARGET expression
# i.e. it is set to 
# TARGETS=$(shell ls *.vsl | sed s/\.vsl/\.s/g)
# after the change it should be
# TARGETS=$(shell ls *.vsl | sed s/\.vsl//g)
# If you leave as is and run make
# assembly files will be generated. 
# individually compile the files by
# running the following command in the shell
#  cc -o easy easy.s -no-pie
# ^ above is for the example easy.vsl
#

%: %.s
	$(CC) -o $@ $< -no-pie

.PRECIOUS: %.s
%.s: %.vsl
	../src/vslc <$*.vsl > $*.s

clean:
	-rm -f *.s

purge: clean
	-rm -f ${TARGETS}
