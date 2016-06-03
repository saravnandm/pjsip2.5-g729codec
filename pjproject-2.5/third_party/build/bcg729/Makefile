include ../../../build.mak
include ../../../build/common.mak

export LIBDIR := ../../lib

RULES_MAK := $(PJDIR)/build/rules.mak

#export G729_CODEC_LIB := ../../lib/libg729codec-$(TARGET_NAME)$(LIBEXT)
export G729_CODEC_LIB := libg729codec-$(TARGET_NAME)$(LIBEXT)

ifeq ($(PJ_SHARED_LIBRARIES),)
else
export G729_CODEC_SONAME := libg729codec.$(SHLIB_SUFFIX)
export G729_CODEC_SHLIB := $(G729_CODEC_SONAME).$(PJ_VERSION_MAJOR)
endif

###############################################################################
# Gather all flags.
#
export _CFLAGS 	:= $(CC_CFLAGS) $(OS_CFLAGS) $(HOST_CFLAGS) $(M_CFLAGS) \
		   $(CFLAGS) $(CC_INC). $(CC_INC)../../bcg729/include \
		   $(CC_INC)../../../pjlib/include
export _CXXFLAGS:= $(_CFLAGS) $(CC_CXXFLAGS) $(OS_CXXFLAGS) $(M_CXXFLAGS) \
		   $(HOST_CXXFLAGS) $(CXXFLAGS)
export _LDFLAGS := $(CC_LDFLAGS) $(OS_LDFLAGS) $(M_LDFLAGS) $(HOST_LDFLAGS) \
		   $(LDFLAGS)

export G729_CODEC_SRCDIR = ../../bcg729/src
export G729_CODEC_OBJS = adaptativeCodebookSearch.o codebooks.o \
	computeAdaptativeCodebookGain.o computeLP.o computeWeightedSpeech.o \
	decodeAdaptativeCodeVector.o decodeFixedCodeVector.o decodeGains.o \
	decodeLSP.o decoder.o encoder.o findOpenLoopPitchDelay.o \
	fixedCodebookSearch.o gainQuantization.o interpolateqLSP.o \
	LP2LSPConversion.o LPSynthesisFilter.o LSPQuantization.o postFilter.o \
	postProcessing.o preProcessing.o qLSP2LP.o utils.o

export G729_CODEC_CFLAGS =  $(_CFLAGS)


export CC_OUT CC AR RANLIB HOST_MV HOST_RM HOST_RMDIR HOST_MKDIR OBJEXT LD LDOUT
###############################################################################
# Main entry
#
# $(TARGET) is defined in os-$(OS_NAME).mak file in current directory.
#
#TARGETS := libg729codec
TARGETS := $(G729_CODEC_LIB) $(G729_CODEC_SONAME)

all: $(TARGETS)

doc:
	cd .. && doxygen docs/doxygen.cfg

dep: depend
distclean: realclean

#.PHONY: dep depend libg729codec clean realclean distclean
.PHONY: all dep depend clean realclean distclean
.PHONY: $(TARGETS)
.PHONY: $(G729_CODEC_LIB) $(G729_CODEC_SONAME)

#libg729codec:
#	$(MAKE) -f $(RULES_MAK) APP=G729_CODEC app=libg729codec $(G729_CODEC_LIB)
libg729codec: $(G729_CODEC_LIB)
$(G729_CODEC_SONAME): $(G729_CODEC_LIB)
$(G729_CODEC_LIB) $(G729_CODEC_SONAME):
	$(MAKE) -f $(RULES_MAK) APP=G729_CODEC app=libg729codec $(subst /,$(HOST_PSEP),$(LIBDIR)/$@)
	
clean print_lib:
	$(MAKE) -f $(RULES_MAK) APP=G729_CODEC app=libg729codec $@

realclean:
	$(subst @@,$(subst /,$(HOST_PSEP),.libg729codec-$(TARGET_NAME).depend),$(HOST_RMR))

	$(MAKE) -f $(RULES_MAK) APP=G729_CODEC app=libg729codec $@

depend:
	$(MAKE) -f $(RULES_MAK) APP=G729_CODEC app=libg729codec $@


