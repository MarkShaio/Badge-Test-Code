#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>

#include "dispatch.h"
#include "mqtt_rx.h"
#include "openbeacon.h"

#include <mosquitto.h>

static dispatch_data *local_dd;
struct mosquitto *mosq_instance;

void my_message_callback(void *obj, const struct mosquitto_message *message)
{
	struct timeval tv;
	int i;
	gettimeofday(&tv, NULL);
	
	uint8_t abc[16];
	char *temp = (char *) message->payload;
	
	for(i=2;i<18;i++)
		sscanf(&temp[i*2], "%2hhx", &abc[i-2]);
	
	uint8_t id[4];
	
	id[0] = 0x0;
	id[1] = 0x0;
	
	for(i=0;i<2;i++)
		sscanf(&temp[i*2], "%2hhx", &id[i+2]);
	
	dispatch_packets(local_dd, abc, 0x10, (uint32_t) id[3], &tv);
	fflush(stdout);
}

void my_connect_callback(void *obj, int result)
{
	struct mosquitto *mosq = obj;
	
	if(!result){
		mosquitto_subscribe(mosq, (uint16_t *) 0, "/beacon2", 0);
	}else{
		fprintf(stderr, "Connect failed\n");
	}
}

void my_subscribe_callback(void *obj, uint16_t mid, int qos_count, const uint8_t *granted_qos)
{
	int i;
	
	printf("Subscribed (mid: %d): %d", mid, granted_qos[0]);
	for(i=1; i<qos_count; i++){
		printf(", %d", granted_qos[i]);
	}
	printf("\n");
}

void my_publish(uint16_t id, double x, double y, double z)
{
	char buffer[100];
	
	sprintf(buffer, "ID:%d, XYZ: %f %f %f", id, x, y, z);
	
	mosquitto_publish(mosq_instance, (uint16_t *)0, "/beacon3", sizeof(buffer), (uint8_t *) buffer, 0, FALSE);
}

void
mloop(dispatch_data *dd) {
	char id[30];

	char *host = "130.102.128.123";
	int port = 1883;
	int keepalive = 60;
	bool clean_session = true;
	struct mosquitto *mosq = NULL;
	
	local_dd = dd;
	
	mosq = mosquitto_new(id, NULL);
	if(!mosq){
		fprintf(stderr, "Error: Out of memory.\n");
		return;
	}
	mosq_instance=mosq;
	mosquitto_connect_callback_set(mosq, my_connect_callback);
	mosquitto_message_callback_set(mosq, my_message_callback);
	mosquitto_subscribe_callback_set(mosq, my_subscribe_callback);
	
	if(mosquitto_connect(mosq, host, port, keepalive, clean_session)){
		fprintf(stderr, "Unable to connect.\n");
		return;
	}
	
	while(mosquitto_loop(mosq, -1) != -1){
	}
	mosquitto_destroy(mosq);
	return;
}
