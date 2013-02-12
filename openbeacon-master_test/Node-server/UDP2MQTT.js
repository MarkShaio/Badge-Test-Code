var mqtt = require('mqttjs');
var dgram = require('dgram');
var server = dgram.createSocket('udp4');

var port = 1883
    host = 'summer.ceit.uq.edu.au'

client = mqtt.createClient(port, host, function(client) {
    client.connect({keepalive: 3000});

    client.on('connack', function(packet) {
        if (packet.returnCode === 0) {
            console.log('connected')
        } else {
            console.log('connack error %d', packet.returnCode);
            process.exit(-1);
        }
    });

    client.on('close', function() {
        process.exit(0);
    });

    client.on('error', function(e) {
        console.log('error %s', e);
        process.exit(-1);
    });
});

server.on("listening", function () {
		  var address = server.address();
		  console.log("server listening " +
					  address.address + ":" + address.port);
		  });

server.on("message", function (msg, rinfo) {
  var buff = new Buffer(msg,'hex')
  //take raw data stream apart
  reader_id = buff.toString('hex', start=4,end=6)
  timestamp = buff.toString('hex', start=12,end=16)
  proto = buff.toString('hex', start=16,end=17)
  oid = buff.toString('hex', start=17,end=19)
  flags = buff.toString('hex', start=19,end=20)
  strength = buff.toString('hex', start=20,end=21)
  oid_last_seen = buff.toString('hex', start=21,end=23)
  power_up_count = buff.toString('hex', start=23,end=25)
  reserved = buff.toString('hex', start=25,end=26)
  seq = buff.toString('hex', start=26,end=30)
console.log("Message Received");  
  //payload written to with new udp badge traffic
  payload = '{"reader_id": "' + reader_id + '", "timestamp": "' + 
  timestamp + '", "proto": "' + proto + '", "oid": "' + oid + 
  '", "flags": "' + flags + '", "strength": "' + strength + 
  '", "oid_last_seen": "' + oid_last_seen + '", "power_up_count": "' +
  power_up_count + '", "reserved": "' + reserved + '", "seq": "' +
  seq + '"}';
  Lpayload = reader_id + "10" + proto + flags + strength + seq +"0000"+ 
oid + 
reserved + "001234";
 
  //if (protocol !=46){  
  client.publish({topic: '/beacon', payload: payload});	 
  client.publish({topic: '/beacon2', payload: Lpayload});
//}
});
console.log(protocol);

server.bind(2342);
