#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib/ghash.h>

#include "dispatch.h"
#include "network_rx.h"
#include "mqtt_rx.h"
#include "openbeacon.h"
#include "readerloc.h"

void
usage(void) {
	printf("OpenBeacon aggregator framework.  Command line usage:\n");
	printf("Packet sources: -U udp_port -M mqtt\n");
	printf("OpenBeacon Tracker estimation output: -H human, -S structured\n");
}

int
main(int argc, char **argv) {
	dispatch_data dd;
	bzero(&dd, sizeof(dd));
	char *topic_in = "/beacon";
	char *topic_out = "/beacon2";
	openbeacon_tracker_data btd;
	bzero(&btd, sizeof(btd));
	beacontracker_init_data(&btd);

	int do_tracker = 0;

	FILE *sourcef = NULL;
	int port = -1;

	enum { SOURCE_NONE
	     , SOURCE_NETWORK
		 , SOURCE_MQTT }
	
	source = SOURCE_NONE;

	char *areasptfile = NULL;
	char *readerloc = NULL;

	int opt;
	while((opt = getopt(argc, argv, "H:S:Q:U:M:h")) != -1) {
		switch (opt) {
		case 'H':
			do_tracker = 1;
			btd.human_out_file = fopen(optarg, "w");
			break;
		case 'S':
			do_tracker = 1;
			btd.structured_out_file = fopen(optarg, "w");
			break;
		case 'Q':
			btd.mqtt_out = 0x01;
			topic_out = optarg;
			break;
		case 'U':
			source = SOURCE_NETWORK;
			if(sourcef) fclose(sourcef);
			port = atoi(optarg);
			break;
		case 'M':
			source = SOURCE_MQTT;
			if(sourcef) fclose(sourcef);
			topic_in = optarg;
			break;
		case 'h':	usage(); return -1;
		}
	}

	switch(source) {
	
	case SOURCE_MQTT:
		fprintf(stderr, "Source == MQTT, Good Choice!\n");
		break;
			
	case SOURCE_NETWORK:
		if(port < 0) {
			fprintf(stderr, "Impossible network port: %d\n", port);
			return -1;
		}
		break;
		
	case SOURCE_NONE:
		fprintf(stderr, "No source specified; nothing to do. Use -h for help\n");
		return -1;
	}

	if(do_tracker) {
		{
			FILE *readersf;

			readerloc = "readers.txt";
			readersf = fopen(readerloc, "r");
			if(!readersf) {
				fprintf(stderr,
					"Unable to open readers file: %s: %m\n",
					readerloc);
				return -1;
			}

			reader_location_load_data(readersf, btd.rxid_location);

			fprintf(stderr, "FYI, I read in %d reader locations\n",
				g_hash_table_size(btd.rxid_location));
			fclose(readersf);
		}

		{
			FILE *areaf;
			areasptfile = "areaspt.txt";
			areaf = fopen(areasptfile, "r");
			if(!areaf) {
				fprintf(stderr,
					"WARNING: Unable to open area SPT file: %s: %m\n",
					areasptfile);
			} else {
				btd.areaspt = spt_read_from_file(areaf);
				if(!btd.areaspt) {
					fprintf(stderr,
						"WARNING: Malformed SPT file %s will produce no area info.\n",
						areasptfile);
				}
			}
		}

		dispatch_set_callback(&dd, OPENBEACON_PROTO_BEACONTRACKER,
								beacontracker_cb, &btd);
	}

	switch(source) {
	case SOURCE_NETWORK: network_loop(&dd, port); break;
	case SOURCE_MQTT: mloop(&dd); break;
	default: fprintf(stderr, "PANIC: Unknown source type?!\n"); return -1;
	}

	/* Make cleanup easier on valgrind */
	beacontracker_cleanup_data(&btd);
#define IFFCL(x) do { if(x) { fclose(x); x = NULL; } } while(0)
	IFFCL(btd.structured_out_file);
	IFFCL(btd.human_out_file);
#undef IFFCL

	return 0;
}
