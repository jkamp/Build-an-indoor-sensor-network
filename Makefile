include src/Makefile.src

CONTIKI_PROJECT = src/main
#CONTIKI_PROJECT = src/packet_buffer_unittest
#CONTIKI_PROJECT = src/queue_buffer_unittest
all: $(CONTIKI_PROJECT)

CONTIKI = third_party/contiki-2.4
DEFINES+=TEAMLK_DEBUG
CFLAGS+=-pedantic
include $(CONTIKI)/Makefile.include
