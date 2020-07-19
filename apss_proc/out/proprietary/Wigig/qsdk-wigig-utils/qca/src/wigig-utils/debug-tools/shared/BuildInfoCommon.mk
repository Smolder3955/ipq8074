BUILD_DATE       := $(shell date +%F)
LAST_CHANGE_ID   := $(shell cd $(REPOSITORY_LOCATION); git log --format="%h" -n1 2>/dev/null)
LAST_CHANGE_DATE := $(shell cd $(REPOSITORY_LOCATION); git log --date=short --format="%cd" -n1 2>/dev/null)

ifneq (,$(EXT_LAST_CHANGE_ID))
	LAST_CHANGE_ID := $(EXT_LAST_CHANGE_ID)
endif

ifneq (,$(EXT_LAST_CHANGE_DATE))
	LAST_CHANGE_DATE := $(EXT_LAST_CHANGE_DATE)
endif

ifneq (,$(EXT_BUILD_ID))
	BUILD_ID := $(EXT_BUILD_ID)
endif

CPPFLAGS += \
	-DBUILD_ID=\"$(BUILD_ID)\" \
	-DBUILD_DATE=\"$(BUILD_DATE)\" \
	-DLAST_CHANGE_ID=\"$(LAST_CHANGE_ID)\" \
	-DLAST_CHANGE_DATE=\"$(LAST_CHANGE_DATE)\" \

$(info ***** Build ID:         $(BUILD_ID))
$(info ***** Build date:       $(BUILD_DATE))
$(info ***** Last change:      $(LAST_CHANGE_ID))
$(info ***** Last change date: $(LAST_CHANGE_DATE))
