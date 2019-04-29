
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
GEN_LIBS = liblwip.a
COMPONENTS_liblwip = api/liblwipapi.a	\
						app/liblwipapp.a	\
						core/liblwipcore.a	\
						core/ipv4/liblwipipv4.a	\
						netif/liblwipnetif.a
endif


#############################################################
# Configuration i.e. compile options etc.
# Target specific stuff (defines etc.) goes in here!
# Generally values applying to a tree are captured in the
#   makefile at its root level - these are then overridden
#   for a subtree within the makefile rooted therein
#
#DEFINES += 
CCFLAGS += -DPBUF_RSV_FOR_WLAN -DEBUF_LWIP

CCFLAGS += -Os
CCFLAGS += -finline-limit=0

CCFLAGS += -fno-optimize-sibling-calls
CCFLAGS += -fno-tree-copy-prop
CCFLAGS += -fno-tree-loop-optimize
CCFLAGS += -fno-tree-sra
CCFLAGS += -free
CCFLAGS += -fmerge-all-constants
CCFLAGS += -fno-jump-tables
CCFLAGS += -fno-tree-copyrename
CCFLAGS += -fprofile-use
CCFLAGS += -fno-tree-dce
CCFLAGS += -fno-tree-forwprop
CCFLAGS += -fno-tree-pre

CCFLAGS += -fno-tree-slsr
CCFLAGS += -fno-branch-probabilities
CCFLAGS += -fno-tree-pta

CCFLAGS += -fno-sched-dep-count-heuristic

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
INCLUDES += -I ./
PDIR := ../$(PDIR)
sinclude $(PDIR)Makefile

