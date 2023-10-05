

if (process.argv.length != 7) {
	console.error("Usage:\nnode test-client.js <server> <port> <name> <secret> <strat>");
	process.exit(1);
}
const [server, port, name, secret, strat] = process.argv.slice(2);

console.log(`Connecting to ${server}:${port} as ${name} (${secret}) with strategy ${strat}.`);

//based on https://gist.github.com/tedmiston/5935757

const net = require('net');

let gatherer = null;
let seeker = null;

function closeGatherer() {
	if (gatherer != null) {
		gatherer.close();
		gatherer = null;
	}
}

function closeSeeker() {
	if (seeker != null) {
		seeker.close();
		seeker = null;
	}
}

function buildClient(role, onView, onGem, onClose) {
	let client = new net.Socket();
	client.connect(parseInt(port), server, function() {
		console.log(`[${role}] connected`);
		//send handshake:
		const data = `${role}${secret}${name}`;
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

			if (type === 'T') {
				const message = data.toString();
				console.log(`[${role}]: '${message}'`);
			} else if (type === 'V') {
				const view = data.toString();
				onView(client, view);
			} else if (type === 'G' && length === 0) {
				onGem(client);
			} else {
				console.log(`[${role}] Ignoring '${type}' of length ${length}`);
			}

			//trim the gotten data:
			recv = Buffer.from(recv.subarray(2 + length));
		}
	});
	
	client.on('close', function() {
		console.log(`[${role}] server closed connection`);
		onClose();
	});

	client.on('error', function(err) {
		console.log(`[${role}] connection error: ${err}`);
		onClose();
	});
}

function randomGatherer() {
	closeGatherer();
	gatherer = buildClient( 'g',
		(client, view) => { //on view
			console.assert(view.length === 9);
			//console.log(`${view[0]}${view[1]}${view[2]}\n${view[3]}${view[4]}${view[5]}\n${view[6]}${view[7]}${view[8]}`);
			//random movement:
			let moves = [];
			if (view[1] == ' ') moves.push('N');
			if (view[3] == ' ') moves.push('W');
			if (view[5] == ' ') moves.push('E');
			if (view[7] == ' ') moves.push('S');
	
			let move = moves[Math.floor(Math.random() * moves.length)];

			client.write('M');
			client.write(Buffer.from([1]));
			client.write(move);
		},
		(client) => { //on gem
			//nothing!
		},
		() => { //on close
			gatherer = null;
		}
	);
}

function randomSeeker(findProb) {
	closeSeeker();
	seeker = buildClient( 's',
		(client, view) => { //on view
			console.assert(view.length === 9);
			//console.log(`${view[0]}${view[1]}${view[2]}\n${view[3]}${view[4]}${view[5]}\n${view[6]}${view[7]}${view[8]}`);
			//random movement:
			let moves = [];
			if (view[1] == ' ') moves.push('N');
			if (view[3] == ' ') moves.push('W');
			if (view[5] == ' ') moves.push('E');
			if (view[7] == ' ') moves.push('S');
	
			let move = moves[Math.floor(Math.random() * moves.length)];

			if (Math.random() < findProb) move = 'f';

			client.write('M');
			client.write(Buffer.from([1]));
			client.write(move);
		},
		(client) => { }, //on gem
		() => { seeker = null; } //on close
	);
}

let gathererDone = false;

function swGatherer() {
	closeGatherer();
	gathererDone = false;
	gatherer = buildClient( 'g',
		(client, view) => { //on view
			console.assert(view.length === 9);
			//move as south+west as possible:
			let move = '.';
			if      (view[7] == ' ') move = 'S';
			else if (view[3] == ' ') move = 'W';
			else gathererDone = true;
	
			client.write('M');
			client.write(Buffer.from([1]));
			client.write(move);
		},
		(client) => { }, //on gem
		() => { gatherer = null; } //on close
	);
}

function swSeeker() {
	closeSeeker();
	seeker = buildClient( 's',
		(client, view) => { //on view
			console.assert(view.length === 9);
			//move as south+west as possible:
			let move = '.';
			if      (view[7] == ' ') move = 'S';
			else if (view[3] == ' ') move = 'W';
			else if (gathererDone) move = 'f';
	
			client.write('M');
			client.write(Buffer.from([1]));
			client.write(move);
		},
		(client) => { }, //on gem
		() => { seeker = null; } //on close
	);
}


function strategize() {
	if (strat === 'random') {
		if (gatherer === null) {
			randomGatherer();
		}
		if (seeker === null) {
			randomSeeker(0.1);
		}
		setTimeout(strategize, 1000);
	} else if (strat === 'sw') {
		if (gatherer === null) {
			swGatherer();
		}
		if (seeker === null) {
			swSeeker(0.1);
		}
		setTimeout(strategize, 1000);
	} else {
		console.log(`Don't know strategy ${strat}.`);
		process.exit(1);
	}
}

strategize();
