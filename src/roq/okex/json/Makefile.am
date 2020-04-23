noinst_LTLIBRARIES = libroq-okex-json.la
libroq_okex_json_la_LDFLAGS = \
	-static \
	$(LIBCONFIG_LIBS) \
	$(ROQ_LIBS)
libroq_okex_json_la_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(LIBCONFIG_CFLAGS) \
	$(ROQ_CFLAGS)
AUTOGEN_CPP = \
	error.cpp \
	method.cpp \
	orderbook.cpp \
	orderbook_item.cpp \
	order.cpp \
	orders.cpp \
	report.cpp \
	report_type.cpp \
	request_type.cpp \
	response.cpp \
	side.cpp \
	status.cpp \
	symbol.cpp \
	symbols.cpp \
	ticker.cpp \
	time_in_force.cpp \
	trades.cpp \
	trades_item.cpp \
	trading_balance.cpp \
	trading_balance_item.cpp \
	type.cpp
AUTOGEN_H = \
	error.h \
	method.h \
	orderbook.h \
	orderbook_item.h \
	order.h \
	orders.h \
	report.h \
	report_type.h \
	request_type.h \
	response.h \
	side.h \
	status.h \
	symbol.h \
	symbols.h \
	ticker.h \
	time_in_force.h \
	trades.h \
	trades_item.h \
	trading_balance.h \
	trading_balance_item.h \
	type.h
CLEANFILES = $(AUTOGEN_CPP) $(AUTOGEN_H)
BUILT_SOURCES = $(CLEANFILES) .gitignore
libroq_okex_json_la_SOURCES = \
	$(AUTOGEN_CPP) \
	utils.cpp

.gitignore: Makefile.am
	echo $(AUTOGEN_CPP) $(AUTOGEN_H) | tr " " "\n" | sort > $@

.json.h:
	@abs_top_builddir@/autogen/autogen.py \
		--namespace=roq/okex/json \
		--output-type json \
		--file-type h \
		$< > $@

.json.cpp:
	@abs_top_builddir@/autogen/autogen.py \
		--namespace=roq/okex/json \
		--output-type json \
		--file-type cpp \
		$< > $@
