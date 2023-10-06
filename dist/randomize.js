const fs = require('fs');

const secrets = [];
{
	const words = fs.readFileSync('../../../wordlist.asc', 'utf-8').split('\n');
	const wordSet = {};
	for (const word of words) {
		if (word.length === 6 && word[5] !== 's') wordSet[word] = true;
	}
	for (const word in wordSet) {
		let swap = word.substr(3,3) + word.substr(0,3);
		if (!(swap in wordSet)) {
			secrets.push(swap);
		}
	}
}

const maps = {"0":1,"1":3,"2":4,"3":5,"4":6,"5":7,"6":8,"7":9,"8":10,"9":11,"10":12,"11":13,"12":14,"13":2,"14":15,"15":16,"16":17,"17":18,"18":19,"19":20,"20":21,"21":22,"22":23,"23":24,"24":25,"25":26,"26":27,"27":28,"28":29,"29":30,"30":31,"31":32,"32":33,"33":34,"34":35,"35":36,"36":37,"37":38,"38":39,"39":40,"40":41,"41":42,"42":43,"43":44,"44":45,"45":46,"46":47,"47":48,"48":49,"49":50,"50":51,"51":52,"52":53,"53":54,"54":55,"55":56,"56":57,"57":58,"58":59,"59":60,"60":61,"61":62,"62":63,"63":64,"64":65,"65":66,"66":67,"67":68,"68":69,"69":70,"70":71,"71":72,"72":73,"73":74,"74":75,"75":76,"76":77,"77":78,"78":79,"79":80,"80":81,"81":82,"82":83,"83":84,"84":85,"85":86,"86":87,"87":88,"88":89,"89":90,"90":91,"91":92,"92":93,"93":94,"94":95,"95":96,"96":97,"97":98,"98":99,"99":100,"100":101,"101":102,"102":103,"103":104,"104":105,"105":106,"106":107,"107":108,"108":109,"109":110,"110":111,"111":112,"112":113,"113":114,"114":115,"115":116,"116":117,"117":118,"118":119,"119":120,"120":121,"121":122,"122":123,"123":124,"124":125,"125":126,"126":127,"127":128,"160":129,"161":130,"162":131,"163":132,"164":133,"165":134,"166":135,"167":136,"168":137,"169":138,"170":139,"171":140,"172":141,"173":142,"174":143,"175":144,"176":145,"177":146,"178":147,"179":148,"180":149,"181":150,"182":151,"183":152,"184":153,"185":154,"186":155,"187":156,"188":157,"189":158,"190":159,"191":160,"192":161,"193":162,"194":163,"195":164,"196":165,"197":166,"198":167,"199":168,"200":169,"201":170,"202":171,"203":172,"204":173,"205":174,"206":175,"207":176,"208":177,"209":178,"210":179,"211":180,"212":181,"213":182,"214":183,"215":184,"216":185,"217":186,"218":187,"219":188,"220":189,"221":190,"222":191,"223":192,"224":193,"225":194,"226":195,"227":196,"228":197,"229":198,"230":199,"231":200,"232":201,"233":202,"234":203,"235":204,"236":205,"237":206,"238":207,"239":208,"240":209,"241":210,"242":211,"243":212,"244":213,"245":214,"246":215,"247":216,"248":217,"249":218,"250":219,"251":220,"252":221,"253":222,"254":223,"255":224,"256":225,"257":226,"258":227,"259":228,"260":229,"261":230,"262":231,"263":232,"264":233,"265":234,"266":235,"267":236,"268":237,"269":238,"270":239,"271":240,"272":241,"273":242,"274":243,"275":244,"276":245,"277":246,"278":247,"279":248,"280":249,"281":250,"282":251,"283":252,"284":253,"285":254,"286":255,"287":256,"288":257,"289":258,"290":259,"291":260,"292":261,"293":262,"294":263,"295":264,"296":265,"297":266,"298":267,"299":268,"300":269,"301":270,"302":271,"303":272,"304":273,"305":274,"306":275,"307":276,"308":277,"309":278,"310":279,"311":280,"312":281,"313":282,"314":283,"315":284,"316":285,"317":286,"318":287,"319":288,"320":289,"321":290,"322":291,"323":292,"324":293,"325":294,"326":295,"327":296,"328":297,"329":298,"330":299,"331":300,"332":301,"333":302,"334":303,"335":304,"336":305,"337":306,"338":307,"339":308,"340":309,"341":310,"342":311,"343":312,"344":313,"345":314,"346":315,"347":316,"348":317,"349":318,"350":319,"351":320,"352":321,"353":322,"354":323,"355":324,"356":325,"357":326,"358":327,"359":328,"360":329,"361":330,"362":331,"363":332,"364":333,"365":334,"366":335,"367":336,"368":337,"369":338,"370":339,"371":340,"372":341,"373":342,"374":343,"375":344,"376":345,"377":346,"378":347,"379":348,"380":349,"381":350,"382":351,"383":352,"402":353,"417":354,"439":355,"506":356,"507":357,"508":358,"509":359,"510":360,"511":361,"536":362,"537":363,"538":364,"539":365,"593":366,"632":367,"710":368,"711":369,"713":370,"728":371,"729":372,"730":373,"731":374,"732":375,"733":376,"894":377,"900":378,"901":379,"902":380,"903":381,"904":382,"905":383,"906":384,"908":385,"910":386,"911":387,"912":388,"913":389,"914":390,"915":391,"916":392,"917":393,"918":394,"919":395,"920":396,"921":397,"922":398,"923":399,"924":400,"925":401,"926":402,"927":403,"928":404,"929":405,"931":406,"932":407,"933":408,"934":409,"935":410,"936":411,"937":412,"938":413,"939":414,"940":415,"941":416,"942":417,"943":418,"944":419,"945":420,"946":421,"947":422,"948":423,"949":424,"950":425,"951":426,"952":427,"953":428,"954":429,"955":430,"956":431,"957":432,"958":433,"959":434,"960":435,"961":436,"962":437,"963":438,"964":439,"965":440,"966":441,"967":442,"968":443,"969":444,"970":445,"971":446,"972":447,"973":448,"974":449,"976":450,"1012":451,"1024":452,"1025":453,"1026":454,"1027":455,"1028":456,"1029":457,"1030":458,"1031":459,"1032":460,"1033":461,"1034":462,"1035":463,"1036":464,"1037":465,"1038":466,"1039":467,"1040":468,"1041":469,"1042":470,"1043":471,"1044":472,"1045":473,"1046":474,"1047":475,"1048":476,"1049":477,"1050":478,"1051":479,"1052":480,"1053":481,"1054":482,"1055":483,"1056":484,"1057":485,"1058":486,"1059":487,"1060":488,"1061":489,"1062":490,"1063":491,"1064":492,"1065":493,"1066":494,"1067":495,"1068":496,"1069":497,"1070":498,"1071":499,"1072":500,"1073":501,"1074":502,"1075":503,"1076":504,"1077":505,"1078":506,"1079":507,"1080":508,"1081":509,"1082":510,"1083":511,"1084":512,"1085":513,"1086":514,"1087":515,"1088":516,"1089":517,"1090":518,"1091":519,"1092":520,"1093":521,"1094":522,"1095":523,"1096":524,"1097":525,"1098":526,"1099":527,"1100":528,"1101":529,"1102":530,"1103":531,"1104":532,"1105":533,"1106":534,"1107":535,"1108":536,"1109":537,"1110":538,"1111":539,"1112":540,"1113":541,"1114":542,"1115":543,"1116":544,"1117":545,"1118":546,"1119":547,"1168":548,"1169":549,"1470":550,"1488":551,"1489":552,"1490":553,"1491":554,"1492":555,"1493":556,"1494":557,"1495":558,"1496":559,"1497":560,"1498":561,"1499":562,"1500":563,"1501":564,"1502":565,"1503":566,"1504":567,"1505":568,"1506":569,"1507":570,"1508":571,"1509":572,"1510":573,"1511":574,"1512":575,"1513":576,"1514":577,"1520":578,"1521":579,"1522":580,"1523":581,"1524":582,"7451":583,"7462":584,"7464":585,"7808":586,"7809":587,"7810":588,"7811":589,"7812":590,"7813":591,"7839":592,"7922":593,"7923":594,"8208":595,"8210":596,"8211":597,"8212":598,"8213":599,"8215":600,"8216":601,"8217":602,"8218":603,"8219":604,"8220":605,"8221":606,"8222":607,"8223":608,"8224":609,"8225":610,"8226":611,"8230":612,"8231":613,"8240":614,"8242":615,"8243":616,"8245":617,"8249":618,"8250":619,"8252":620,"8254":621,"8255":622,"8256":623,"8260":624,"8276":625,"8308":626,"8309":627,"8310":628,"8311":629,"8312":630,"8313":631,"8314":632,"8315":633,"8319":634,"8321":635,"8322":636,"8323":637,"8324":638,"8325":639,"8326":640,"8327":641,"8328":642,"8329":643,"8330":644,"8331":645,"8355":646,"8356":647,"8359":648,"8362":649,"8364":650,"8453":651,"8467":652,"8470":653,"8482":654,"8486":655,"8494":656,"8528":657,"8529":658,"8531":659,"8532":660,"8533":661,"8534":662,"8535":663,"8536":664,"8537":665,"8538":666,"8539":667,"8540":668,"8541":669,"8542":670,"8592":671,"8593":672,"8594":673,"8595":674,"8596":675,"8597":676,"8616":677,"8706":678,"8709":679,"8710":680,"8712":681,"8719":682,"8721":683,"8722":684,"8725":685,"8729":686,"8730":687,"8734":688,"8735":689,"8745":690,"8747":691,"8776":692,"8800":693,"8801":694,"8804":695,"8805":696,"8857":697,"8960":698,"8962":699,"8976":700,"8992":701,"8993":702,"9472":703,"9474":704,"9484":705,"9488":706,"9492":707,"9496":708,"9500":709,"9508":710,"9516":711,"9524":712,"9532":713,"9552":714,"9553":715,"9554":716,"9555":717,"9556":718,"9557":719,"9558":720,"9559":721,"9560":722,"9561":723,"9562":724,"9563":725,"9564":726,"9565":727,"9566":728,"9567":729,"9568":730,"9569":731,"9570":732,"9571":733,"9572":734,"9573":735,"9574":736,"9575":737,"9576":738,"9577":739,"9578":740,"9579":741,"9580":742,"9600":743,"9601":744,"9604":745,"9608":746,"9612":747,"9616":748,"9617":749,"9618":750,"9619":751,"9632":752,"9633":753,"9642":754,"9643":755,"9644":756,"9650":757,"9658":758,"9660":759,"9668":760,"9674":761,"9675":762,"9679":763,"9688":764,"9689":765,"9702":766,"9786":767,"9787":768,"9788":769,"9792":770,"9794":771,"9824":772,"9827":773,"9829":774,"9830":775,"9834":776,"9835":777,"10003":778,"64257":779,"64258":780,"65533":781};

const chars = [];
for (let name in maps) {
	let cp = parseInt(name);
	if (cp > 32 && cp < 128) {
		chars.push(String.fromCodePoint(cp));
	} else {
		cp = cp.toString(16);
		while (cp.length < 4) cp = '0' + cp;
		chars.push('U+' + cp);
	}
}

const list = fs.readFileSync(process.argv[2], 'utf-8').split('\n');

const out = [];

for (const line of list) {
	if (line === '') continue;
	const toks = line.split(/\s+/);
	while (toks.length < 4) toks.push('x');
	if (toks[1] == 'x') {
		toks[1] = secrets[Math.floor(secrets.length * Math.random())];
	}
	if (toks[2] === 'x') {
		toks[2] = chars[Math.floor(chars.length * Math.random())];
	}
	if (toks[3] === 'x') {
		let r,g,b;
		do {
			r = Math.floor(Math.random() * 256);
			g = Math.floor(Math.random() * 256);
			b = Math.floor(Math.random() * 256);
		} while (r < 127 && g < 127 && b < 127);
		function hex(n) {
			let ret = n.toString(16);
			while (ret.length < 2) ret = '0' + ret;
			return ret;
		}
		toks[3] = ('#' + hex(r) + hex(g) + hex(b));
	}
	out.push(toks.join(' '));
}

fs.writeFileSync(process.argv[2], out.join('\n') + '\n');
