
const [server, port, players_txt] = process.argv.slice(2);


const fs = require('fs');
const child_process = require('child_process');

const lines = fs.readFileSync(players_txt, 'utf8').split('\n');

const strats = ['random', 'sw'];

for (const line of lines) {
	if (line == '') continue;
	const [name, secret, avatar, color] = line.split(/\s+/);

	const strat = strats[Math.floor(Math.random() * strats.length)];

	//console.log(name + ' ' + secret);
	//console.log(secret.substr(3,3) + secret.substr(0,3));
	child_process.spawn('node', [
		'test-client.js', server, port, name, secret, strat
	], {stdio:'inherit'} );
}
