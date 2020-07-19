/**
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All rights reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

function imageExists(url, callback) {
  var img = new Image();
  img.onload = function() { callback(true); };
  img.onerror = function() { callback(false); };
  img.src = url;
}
var imageUrl = titleimage;
imageExists(imageUrl, function(exists) {
	if(exists){
		var theImageDiv = document.getElementById("titleimage");
		var img=document.createElement("img");
		img.src=titleimage;
		theImageDiv.appendChild(img);
	}
});

var theFootnoteByDiv = document.getElementById("footnote");
var footcontent = document.createTextNode(footnote);
theFootnoteByDiv.appendChild(footcontent);

var svg = build_svg();
var clientinfo = {};
var APinfo;
var type;
var numcount = 0;
var overload24 = 0;
var overload5 = 0;
var n = 40,
    duration = 750;
var t =0;
var uebws = [];

//console.log('Message', MESSAGE_TYPES);
var wsurl = "ws://" + window.location.host + "/websocket";
ws = new WebSocket(wsurl);
ws.onopen = function(e) {
	console.log('Connection to server opened');
}
setInterval(ws.onmessage, 3000);
ws.onmessage = function(e) {
	var msg = e.data;
	var fullMessage = JSON.parse(e.data);
	var type = fullMessage.type;
	switch(MESSAGE_TYPES[type-1]){
		case "AP_INFO":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
			APinfo =fullMessage.payload;
			console.log('APinfo',APinfo);
			createAPI();
			getAPIoverload();
			creategauge('gauge24GHZ',gauge1config.x,gauge1config.y,gauge1config.size,'2.4GHz',overload24);
			creategauge('gauge5LGHZ',gauge2config.x,gauge2config.y,gauge2config.size,'5LGHz',overload5l);
			creategauge('gauge5HGHZ',gauge3config.x,gauge3config.y,gauge3config.size,'5HGHz',overload5h);
		break;
		case "CLIENT_INFO":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
			var info = fullMessage.payload;
			if(clientarray.length){
				for(var cname in info){
					if(clientarray.indexOf(cname)>-1){
						clientinfo[cname] = info[cname];
					}
				}
			}else{
				clientinfo = fullMessage.payload;
			}
			assignconfig(clientinfo);
			createdefaultue();
		break;
		case "FULL_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
		break;
		case "TEXT":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
		break;
		case "TPUT_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
			Handle_TPUT_UPDATE(fullMessage.payload);
		break;
		case "ASSOCIATION_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
			Handle_UE_UPDATE(fullMessage.payload);
		break;
		case "STEERING_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
		break;
		case "UTIL_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
			Handle_UPDATE_GAUGES(fullMessage.payload);

		break;
		case "BLACKOUT_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
		break;
		case "OVERLOAD_THRESH":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
		break;
		case "OVERLOAD_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
		break;
		case "STEERING_ATTEMPT":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
		break;
		case "AP_INFO":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
		break;
		case "FLAG_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
			Handle_UE_FLAG_UPDATE(fullMessage.payload)
		break;
		case "PATH_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
			//Handle_UE_PATHUPDATE(fullMessage.payload);
		break;
	}
	//createAPI();
}

function createAPI() {
	for(var key in APinfo){
		var api_id = key;
		createServingVap(api_id,routerconfig);
		drawAPI(api_id);
		break;
	}
}


function dummymessage()
{
	var message;
	var number = Math.random() *100;
	var theNumber = Math.round(number);
	message = {'gauge24GHZ': theNumber};
	Handle_TPUT_UPDATE(message);
	var number = Math.random() *100;
	var theNumber2 = Math.round(number);
	message = {'gauge52GHZ': theNumber2};
	Handle_TPUT_UPDATE(message);
}
function getAPIoverload() {
	for(var key in APinfo){
		var api_id = key;
		var API = APinfo[key];
		//console.log('api_id',api_id);
		//console.log('API',API);
		if((API.type).toLowerCase() == apitype[0]){
			for(var vaps in API.vaps){
				var vapsinfo = API.vaps[vaps];
				overload24 = vapsinfo.overload;
				overload5l = vapsinfo.overload;
				overload5h = vapsinfo.overload;
			}
		}
		if((API.type).toLowerCase() == apitype[1]){
			for(var vaps in API.vaps){
				var vapsinfo = API.vaps[vaps];
				overload24 = vapsinfo.overload;
				overload5 = vapsinfo.overload;
			}
		}
	}
}

function build_svg() {
	var svg = d3.select("#band").append("svg")
	.attr("id", "Container")
	.attr("width", 1600)
	.attr("height", 902)
	.append("g")
	.on("click", mousemove)
	return svg;
}
function mousemove(d, i) {
	console.log(d3.mouse(this));
}

function assignconfig() {
	var count=0;
	for(var key in clientinfo){
			clientinfo[key].config = ueconfig[count];
			clientinfo[key].config.color = uecolor[count];
			clientinfo[key].config.linecolor = uelinecolor[count];
			clientinfo[key].data= [];
			count++;
	}
}

function Handle_TPUT_UPDATE(message)
{
	for(var key in message){
		var ue_id = key;
		var tputvalue = message[key];
		updateuembps(ue_id,tputvalue);

		//console.log('message',message);
		//if(checkifdatachange(ue_id,tputvalue))
		//	tick(ue_id,tputvalue);
	}
	addtotalbw();
}
function checkifdatachange(ue_id,tputvalue)
{
	for(var key in clientinfo){
		if(key == ue_id){
			var length = clientinfo[key].data.length;
			if(length){
				var value = clientinfo[key].data[length-1];
				if(value == tputvalue)
					return false;
				else
					return true;

			}else
				return true;
		}
	}
}

function Handle_UE_UPDATE(message)
{
	for(var key in message){
		var ue_id = key;
		var updateinfo = message[key];
		if(ue_id ){
			updateue(ue_id,updateinfo,APinfo,clientinfo);
		}
	}
}
function Handle_UPDATE_GAUGES(message)
{
	for(var key in message){
		var api_id = key;
		var value = message[key];

		switch(key) {
		case apiband[0]:
			updategauge('gauge24GHZ',value);
		break;
		case apiband[1]:
			updategauge('gauge5HGHZ',value);
		break;
		case apiband[2]:
			updategauge('gauge5LGHZ',value);
		break;
		}
	}
}
function Handle_UE_FLAG_UPDATE(message)
{
	if (message["flags_updated"]["polluted"]) {
		clients = message["clients"];
		for(var key in clients) {
			var ue_id = key;
			var updateinfo = clients[key];
			if (ue_id){
				updateueflag(ue_id,updateinfo,APinfo,clientinfo)
			}
		}
	}
}
