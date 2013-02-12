#ifndef _MQTT_RX_H_
#define _MQTT_RX_H_

#include "openbeacon.h"
#include "dispatch.h"

void my_publish(uint16_t id, double x, double y, double z);
void mloop(dispatch_data *);

#endif
