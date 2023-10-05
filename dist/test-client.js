

const name = 'blue';
const secret = 'Secret';

//based on https://gist.github.com/tedmiston/5935757

const net = require('net');

const client = new net.Socket();
client.connect(15466, '127.0.0.1', function() {
	console.log('connected');
	//send handshake:
	const data = `g${secret}${name}`;
	client.write('H');
	client.write(Buffer.from([data.length]));
	client.write(data);
});

let recv = Buffer.from([]);

client.on('data', function(data) {
	recv = Buffer.concat([recv, data]);
	//console.log('recv is: ' + recv);
	while (recv.length >= 2) {
		const type = recv.toString('utf8',0,1);
		const length = recv[1];

		//wait for more data:
		if (recv.length < 2 + length) break;

		const data = recv.subarray(2, 2+length);

		if (type == 'T') {
			const message = data.toString();
			console.log(`Server said: '${message}'`);
		} else if (type == 'V') {
			const view = data.toString();
			console.assert(view.length === 9);
			console.log(`${view[0]}${view[1]}${view[2]}\n${view[3]}${view[4]}${view[5]}\n${view[6]}${view[7]}${view[8]}`);
			//random movement:
			let moves = [];
			if (view[1] == ' ') moves.push('N');
			if (view[3] == ' ') moves.push('W');
			if (view[5] == ' ') moves.push('E');
			if (view[7] == ' ') moves.push('S');

			const move = moves[Math.floor(Math.random() * moves.length)];

			console.log(`moving ${move}`);

			client.write('M');
			client.write(Buffer.from([1]));
			client.write(move);
		} else {
			console.log(`Ignoring '${type}' of length ${length}`);
		}

		//trim the gotten data:
		recv = Buffer.from(recv.subarray(2 + length));
	}
});

client.on('close', function() {
	console.log('server closed connection');
});
