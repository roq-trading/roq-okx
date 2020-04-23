# SO 35998856
check_LTLIBRARIES = libgtest.la
libgtest_la_SOURCES = \
        $(top_srcdir)/third_party/googletest/googletest/src/gtest-all.cc
libgtest_la_CPPFLAGS = \
        -I$(top_srcdir)/third_party/googletest/googletest/include \
        -I$(top_srcdir)/third_party/googletest/googletest
libgtest_la_LDFLAGS = -pthread

TESTS = roq-okex-test
check_PROGRAMS = roq-okex-test

roq_okex_test_LDADD = \
	libgtest.la \
	$(top_builddir)/src/roq/okex/json/libroq-okex-json.la \
	$(DATE_LIBS) \
	$(ROQ_CORE_LIBS) \
	$(ROQ_API_LIBS) \
	$(FMT_LIBS) \
	$(DATE_LIBS)
roq_okex_test_CPPFLAGS = \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/third_party/googletest/googletest/include \
	$(ROQ_API_CFLAGS) \
	$(ROQ_CORE_CFLAGS) \
	$(FMT_CFLAGS) \
	$(DATE_CFLAGS)
nodist_roq_okex_test_SOURCES = \
	main.cpp
