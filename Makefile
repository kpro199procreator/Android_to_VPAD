#-------------------------------------------------------------------------------
# android_to_vpad — Makefile
# Fase 1: soporte TCP básico para input desde Android / Termux
#-------------------------------------------------------------------------------

WUPS_PLUGIN_NAME := android_to_vpad
TARGET           := $(WUPS_PLUGIN_NAME)

#-------------------------------------------------------------------------------
# Toolchain roots (auto-detect para builds locales o CI)
#-------------------------------------------------------------------------------
DEVKITPRO ?= /opt/devkitpro
WUT_ROOT  ?= $(DEVKITPRO)/wut
WUPSDIR   ?= $(DEVKITPRO)/wups

# Fallback útil cuando DEVKITPRO no está exportado y WUPS está en /wups
ifeq ($(wildcard $(WUPSDIR)/share/wups_rules),)
ifneq ($(wildcard /wups/share/wups_rules),)
WUPSDIR := /wups
endif
endif

#-------------------------------------------------------------------------------
# Fuentes e includes
#-------------------------------------------------------------------------------
SOURCES  := src
INCLUDES := src

#-------------------------------------------------------------------------------
# Flags
#-------------------------------------------------------------------------------
CFLAGS   :=
CXXFLAGS := -std=c++20

#-------------------------------------------------------------------------------
# Librerías
#-------------------------------------------------------------------------------
LIBS := \
    -lwups         \
    -lwut          \
    -lcoreinit     \
    -lnsysnet      \
    -lnotifications

#-------------------------------------------------------------------------------
# Directorios de librerías
#-------------------------------------------------------------------------------
LIBDIRS := $(WUPSDIR) $(WUT_ROOT) $(DEVKITPRO)/portlibs/wiiu

#-------------------------------------------------------------------------------
# Reglas WUPS — debe ser la última línea
#-------------------------------------------------------------------------------
ifeq ($(wildcard $(WUPSDIR)/share/wups_rules),)
$(error WUPSDIR not set or wups_rules not found at $(WUPSDIR)/share/wups_rules)
endif
include $(WUPSDIR)/share/wups_rules
