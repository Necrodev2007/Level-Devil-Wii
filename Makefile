# ---------------------------------------------------------------------------------
# Makefile FINAL - VERSIÓN AUDIO + PNG
# ---------------------------------------------------------------------------------

.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

# --- HERRAMIENTAS FORZADAS ---
PREFIX  := powerpc-eabi-
CC      := $(PREFIX)gcc
CXX     := $(PREFIX)g++
LD      := $(PREFIX)g++
AR      := $(PREFIX)ar
OBJCOPY := $(PREFIX)objcopy
# -----------------------------

TARGET      :=  devil
BUILD       :=  build
SOURCES     :=  source
DATA        :=  data

# --- RUTAS FORZADAS (CORREGIDAS) ---
MY_LIBOGC_INC := /c/devkitPro/libogc/include
MY_LIBOGC_LIB := /c/devkitPro/libogc/lib/wii
MY_PORTLIBS   := /c/devkitPro/portlibs/ppc

# --- LISTA DE LIBRERÍAS (Nota: -lasnd ya estaba, eso es bueno) ---
LIBS    :=  -lgrrlib -lpngu -lpng -ljpeg -lfreetype -lmad -lasnd -lwiiuse -lbte -logc -lm -lz

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   :=  $(CURDIR)/$(TARGET)
export VPATH    :=  $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                    $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR  :=  $(CURDIR)/$(BUILD)

export LIBDIRS  :=  $(MY_LIBOGC_LIB) $(MY_PORTLIBS)/lib
export INCLUDES :=  $(foreach dir,$(SOURCES),-I$(CURDIR)/$(dir)) \
                    -I$(CURDIR)/$(BUILD) \
                    -I$(MY_PORTLIBS)/include \
                    -I$(MY_LIBOGC_INC)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo cleaning ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).dol $(TARGET).map

else

DEPENDS :=  $(OFILES:.o=.d)
CFLAGS      :=  -g -O2 -Wall $(MACHDEP) -Wno-strict-aliasing $(INCLUDES)
CXXFLAGS    :=  $(CFLAGS)
LDFLAGS     :=  $(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBPATHS    :=  -L$(MY_LIBOGC_LIB) -L$(MY_PORTLIBS)/lib
LDLIBS      :=  $(LIBS)

# --- BUSCAR ARCHIVOS ---
CFILES      :=  $(foreach dir,$(SOURCES),$(notdir $(wildcard ../$(dir)/*.c)))
# Busca PNGs
PNGFILES    :=  $(foreach dir,$(DATA),$(notdir $(wildcard ../$(dir)/*.png)))
# [NUEVO] Busca WAVs
WAVFILES    :=  $(foreach dir,$(DATA),$(notdir $(wildcard ../$(dir)/*.wav)))

# --- LISTA DE OBJETOS ---
# Se añaden los WAVs convertidos (.wav.o) a la lista final
OFILES      :=  $(CFILES:.c=.o) $(PNGFILES:.png=.png.o) $(WAVFILES:.wav=.wav.o)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
	@echo linking ...
	@$(LD) $(OFILES) $(LIBPATHS) $(LDLIBS) $(LDFLAGS) -o $@

%.o: %.c
	@echo compiling $<
	@$(CC) $(CFLAGS) -c $< -o $@

%.o : %.png
	@echo converting image $<
	@$(bin2o)

%.png.o : %.png
	@echo converting image $<
	@$(bin2o)

# [NUEVO] Regla para convertir WAV a objeto
%.wav.o : %.wav
	@echo converting audio $<
	@$(bin2o)

# --- DEPENDENCIAS ---
# Main depende de todas las imagenes Y todos los sonidos
main.o : $(PNGFILES:.png=.png.o) $(WAVFILES:.wav=.wav.o)

-include $(DEPENDS)

endif