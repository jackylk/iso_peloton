## Makefile.am -- Process this file with automake to produce Makefile.in
 
######################################################################
# BENCHMARK
######################################################################

######################################################################
# HYADAPT
######################################################################
 
hyadapt_SOURCES =  \
					backend/benchmark/hyadapt/hyadapt.cpp \
                    backend/benchmark/hyadapt/configuration.cpp \
                    backend/benchmark/hyadapt/workload.cpp \
                    backend/benchmark/hyadapt/loader.cpp

hyadapt_LDFLAGS =
hyadapt_CPPFLAGS = -I. -I$(top_srcdir)/src -I.. $(postgres_common_INCLUDES) $(AM_CPPFLAGS)  \
				   $(third_party_INCLUDES) \
				   -I$(srcdir)/backend/benchmark
 
hyadapt_LDADD = libpelotonpg.la libpeloton.la -lpthread

######################################################################
# YCSB
######################################################################

ycsb_SOURCES =  \
				backend/benchmark/ycsb/ycsb.cpp \
                backend/benchmark/ycsb/configuration.cpp \
                backend/benchmark/ycsb/workload.cpp \
                backend/benchmark/ycsb/loader.cpp
                                       
ycsb_LDFLAGS =
ycsb_CPPFLAGS = -I. -I$(top_srcdir)/src -I.. $(postgres_common_INCLUDES) $(AM_CPPFLAGS)  \
				$(third_party_INCLUDES) \
				-I$(srcdir)/backend/benchmark

ycsb_LDADD = libpelotonpg.la libpeloton.la
                                   
######################################################################
# TPCC
######################################################################

tpcc_SOURCES =  \
				backend/benchmark/tpcc/tpcc.cpp \
                backend/benchmark/tpcc/configuration.cpp \
                backend/benchmark/tpcc/workload.cpp \
                backend/benchmark/tpcc/loader.cpp
                                       
tpcc_LDFLAGS =
tpcc_CPPFLAGS = -I. -I$(top_srcdir)/src -I.. $(postgres_common_INCLUDES) $(AM_CPPFLAGS)  \
				$(third_party_INCLUDES) \
				-I$(srcdir)/backend/benchmark

tpcc_LDADD = libpelotonpg.la libpeloton.la     