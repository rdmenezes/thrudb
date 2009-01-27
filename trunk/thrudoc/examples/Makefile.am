
all: gen-perl/Thrudoc.pm

gen-perl/Thrudoc.pm: $(top_srcdir)/src/Thrudoc.thrift
	$(THRIFT) --gen cpp --gen java -php --gen py --gen rb --gen perl $<

clean:
	rm -fr gen-*
