
#############################################################
# Required variables for each makefile
# Discard this section from all parent makefiles
# Expected variables (with automatic defaults):
#   CSRCS (all "C" files in the dir)
#   SUBDIRS (all subdirs with a Makefile)
#   GEN_LIBS - list of libs to be generated ()
#   GEN_IMAGES - list of images to be generated ()
#   COMPONENTS_xxx - a list of libs/objs in the form
#     subdir/lib to be extracted and rolled up into
#     a generated lib/image xxx.a ()
#
ifndef PDIR
UP_EXTRACT_DIR = ..
GEN_LIBS = libmbedtls.a
COMPONENTS_libmbedtls = library/liblibrary.a platform/libplatform.a	app/libapp.a
endif


#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#
DEFINES += -DMBEDTLS_CONFIG_FILE='"config_esp.h"'
#CCFLAGS += --rename-section .text=.irom0.text --rename-section .literal=.irom0.literal

CCFLAGS += -Os

CCFLAGS += -fno-tree-phiprop
CCFLAGS += -fno-ipa-sra
CCFLAGS += -fno-sched-critical-path-heuristic
CCFLAGS += -fno-ipa-cp
CCFLAGS += -fno-move-loop-invariants
CCFLAGS += -fipa-pta
CCFLAGS += -fno-tree-scev-cprop
CCFLAGS += -fno-rerun-cse-after-loop
CCFLAGS += -fmerge-all-constants
CCFLAGS += -fno-tree-loop-ivcanon
CCFLAGS += -fno-jump-tables
CCFLAGS += -fno-tree-copy-prop
CCFLAGS += -fno-tree-pre

CCFLAGS += -fschedule-insns
CCFLAGS += -free
CCFLAGS += -fno-tree-bit-ccp
CCFLAGS += -fprofile-use
CCFLAGS += -fno-tree-loop-im

CCFLAGS += -fno-tree-slp-vectorize
CCFLAGS += -fno-sched-interblock
CCFLAGS += -fno-rename-registers
CCFLAGS += -fsched-pressure
CCFLAGS += -fno-sched-dep-count-heuristic

#CCFLAGS += -flive-range-shrinkage

CCFLAGS += $(OPT)

#############################################################
# Recursion Magic - Don't touch this!!
#
# Each subtree potentially has an include directory
#   corresponding to the common APIs applicable to modules
#   rooted at that subtree. Accordingly, the INCLUDE PATH
#   of a module can only contain the include directories up
#   its parent path, and not its siblings
#
# Required for each makefile to inherit from the parent
#

INCLUDES := $(INCLUDES) -I $(PDIR)include
INCLUDES += -I ../$(PDIR)include/lwip/posix
INCLUDES += -I ../$(PDIR)include/mbedtls
INCLUDES += -I ./
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

