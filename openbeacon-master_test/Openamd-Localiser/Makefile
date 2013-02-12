CFLAGS+=--std=c99 -Wall -Werror -O2 -g
CFLAGS+= -I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include/
LDFLAGS+= -lpcap -lglib-2.0 -lmosquitto

ESTIMATOR_CFLAGS=-funit-at-a-time

TARGETS=estimator

ESTIMATOR_SOURCES= \
		dispatch.c \
		network_rx.c \
		mqtt_rx.c \
		util.c \
		estimator.c \
		openbeacon.c \
		readerloc.c \
		spaceparttree.c \

DEP_SUFFIX=dep
DEPCHECK_TARGET=.depcheck
all: $(TARGETS)

estimator: $(ESTIMATOR_SOURCES) $(DEPCHECK_TARGET)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(ESTIMATOR_CFLAGS) -o $@ \
	$(filter-out $(DEPCHECK_TARGET),$^)

.PHONY: clean
clean:
	rm -f *.o
	rm -f *.$(DEP_SUFFIX) $(DEPCHECK_TARGET)
	rm -f $(TARGETS)

%.$(DEP_SUFFIX): %.c
	$(CC) $(CFLAGS) $(INCLUDES) -M -MP -MF $@ -MT $(DEPCHECK_TARGET) $<

ifeq (0,$(words $(filter %clean,$(MAKECMDGOALS))))
DO_INCLUDE_DEPS=1
endif

ifeq (1,$(DO_INCLUDE_DEPS))
-include $(ESTIMATOR_SOURCES:%.c=%.$(DEP_SUFFIX))
endif

$(DEPCHECK_TARGET):
	touch $@
