var initialised = false;

function appMessageAck(e) {
    console.log("options sent to Pebble successfully");
}

function appMessageNack(e) {
    console.log("options not sent to Pebble: " + e.error.message);
}

Pebble.addEventListener("ready", function() {
    initialised = true;
});

Pebble.addEventListener("showConfiguration", function() {
    var options = JSON.parse(window.localStorage.getItem('nadir_opt'));
    console.log("read options: " + JSON.stringify(options));
    console.log("showing configuration");
    if (options == null) {
        var uri = 'http://panicman.byto.de/config_nadir.html?title=Nadir';
    } else {
        var uri = 'http://panicman.byto.de/config_nadir.html?title=Nadir' + 
			'&inv=' + encodeURIComponent(options['inv']) + 
			'&anim=' + encodeURIComponent(options['anim']) + 
			'&sep=' + encodeURIComponent(options['sep']) +
			'&datefmt=' + encodeURIComponent(options['datefmt']) + 
			'&vibr=' + encodeURIComponent(options['vibr']);
    }
	console.log("Uri: "+uri);
    Pebble.openURL(uri);
});

Pebble.addEventListener("webviewclosed", function(e) {
    console.log("configuration closed");
    if (e.response != '') {
        var options = JSON.parse(decodeURIComponent(e.response));
        console.log("storing options: " + JSON.stringify(options));
        window.localStorage.setItem('nadir_opt', JSON.stringify(options));
        Pebble.sendAppMessage(options, appMessageAck, appMessageNack);
    } else {
        console.log("no options received");
    }
});
