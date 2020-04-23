TESTS = roq-okex-benchmark
bin_PROGRAMS = roq-okex-benchmark
roq_okex_benchmark_LDADD = \
	$(top_builddir)/src/roq/okex/json/libroq-okex-json.la \
	$(DATE_LIBS) \
	$(ROQ_API_LIBS) \
	$(ROQ_CORE_LIBS) \
	$(FMT_LIBS) \
	$(BENCHMARK_LIBS)  \
	$(RT_LIBS) \
	-lpthread
roq_okex_benchmark_CFLAGS = -pthread
roq_okex_benchmark_CPPFLAGS = \
	-I$(top_builddir)/src \
	$(ROQ_API_CFLAGS) \
	$(ROQ_CORE_CFLAGS) \
	$(BENCHMARK_CFLAGS) \
	$(FMT_CFLAGS) \
	$(DATE_CFLAGS)
nodist_roq_okex_benchmark_SOURCES = \
	main.cpp
