#######################################
# The list of libraries we are building seperated by spaces.
# The 'lib_' indicates that these build products will be installed
# in the $(libdir) directory. For example /usr/lib
lib_LTLIBRARIES = libUCP2PCtrl.la

#######################################
# Build information for each library

# Sources for libUCP2PCtrl
libUCP2PCtrl_la_SOURCES = libUCP2PCtrl.c \
	strDup.cpp \
	Base64.cpp \
	our_md5.c \
	our_md5hl.c \
	DigestAuthentication.cpp \
	Queue.cpp \
	CMyQueue.cpp \
	CMyPing.cpp \
	CommUDP.cpp \
	CommTCP.cpp \
	H264RTP_UnPack.cpp \
	CRtspClient.cpp \
	CRtspServer.cpp \
	UCP2PCtrl.cpp

# Linker options libTestProgram
libUCP2PCtrl_la_LDFLAGS = 

# Compiler options. Here we are adding the include directory
# to be searched for headers included in the source code.
libUCP2PCtrl_la_CPPFLAGS = -I$(top_srcdir)/include

# 附加的依赖库
libUCP2PCtrl_la_LIBADD= -lpthread
