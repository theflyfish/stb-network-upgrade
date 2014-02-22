DVD_MAKE_VERSION := 2
ifdef IN_OBJECT_DIR

##TMTM
export DVD_TRANSPORT = STPTI

include $(DVD_MAKE)/generic.mak

# Exported libraries and headers
# ------------------------------
EXPORTS := netupdate.lib

# Resulting target
# ----------------
TARGETS :=netupdate.lib

# Local objects
# -------------
OBJS := netupdate.tco des.tco

include $(DVD_MAKE)/defrules.mak


# Rules to build component-specific targets
# =========================================
CFLAGS := $(CFLAGS) $(OPTIONAL_CFLAGS) -cpp

##CFLAGS += -DINTEGRATE_TUNER_MODULE

#----------------------------------------------------------------------
# include path for shared objects
INCLUDES += -I $(subst $(BAD_SLASH),$(GOOD_SLASH),../../../shared/ST_$(DVD_FRONTEND))
INCLUDES += -I $(subst $(BAD_SLASH),$(GOOD_SLASH),../../../shared/ST_$(DVD_FRONTEND)/$(DVD_FRONTEND))
INCLUDES += -I $(subst $(BAD_SLASH),$(GOOD_SLASH),../../../srinclude)


# Build TARGETS
# -------------
$(TARGETS) : $(OBJS)
	@$(ECHO) Building $@
	$(BUILD_LIBRARY)

display_variables:
	@$(ECHO) Display Build System variables
	@$(ECHO) SUBDIR_LIBS = $(SUBDIR_LIBS)
	@$(ECHO) SUBDIR_LIBRARY_PATH = $(SUBDIR_LIBRARY_PATH)
	@$(ECHO) INCLUDE_PATH = $(INCLUDE_PATH)

# Clean is mapped on clean_all to allow its propagation
clean: $(SUBDIR_CLEAN)
	@$(ECHO) Cleaning $(TARGETS)
	-$(RM) $(TARGETS)
	-$(RM) $(OBJS)

###################################
# Dependency Generator Target
# by Louie
# run "gmake dep"
# to auto create a dependency check
# file for this module
###################################
-include ../../depends.mak
dep: 
	@$(ECHO) Generating dependencies file depends.mak
	-$(RM) $(OBJS)	
	$(MAKE) -C ../.. $(OBJS) CFLAGS="$(CFLAGS) -v -C" > compile.log
	depends $(@D)/compile.log $(@D)/../../depends.mak
	-$(RM) $(OBJS) compile.log
####### DEPENDS.MAK ###############

else

include $(DVD_MAKE)/builddir.mak

endif
# EOF

