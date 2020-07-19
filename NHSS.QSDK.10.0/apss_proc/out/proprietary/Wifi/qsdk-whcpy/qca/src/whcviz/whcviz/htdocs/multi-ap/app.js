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

var theTextDiv = document.getElementById("title");
var content = document.createTextNode(title);
theTextDiv.appendChild(content);
var thePowerByDiv = document.getElementById("powerby");
var powercontent = document.createTextNode(powerby);
thePowerByDiv.appendChild(powercontent);

var svg = build_svg();
var clientinfo = {};
var APinfo;
var vaps = {};
var type;
var numcount = 0;
var overload24 = 0;
var overload5 = 0;

//console.log('Message', MESSAGE_TYPES);
var wsurl = "ws://" + window.location.host + "/websocket";
ws = new WebSocket(wsurl);
ws.onopen = function(e) {
	console.log('Connection to server opened');
}

ws.onmessage = function(e) {
	 var fullMessage = JSON.parse(e.data);
	 var type = fullMessage.type;
	 switch(MESSAGE_TYPES[type-1]){
		 case "AP_INFO":
			 APinfo =fullMessage.payload;
			//console.log('APinfo',APinfo);
			 createAPI();
			 getAPIoverload();
			 creategauge('gauge24GHZ',gauge1config.x,gauge1config.y,gauge1config.size,'2.4GHz',overload24);
			 creategauge('gauge5GHZ',gauge2config.x,gauge2config.y,gauge2config.size,'5GHz',overload5);
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
			 init();
			 createdefaultue();
		 break;
		 case "FULL_UPDATE":
			 //console.log(MESSAGE_TYPES[type-1], fullMessage);
		 break;
		 case "TEXT":
			 //console.log(MESSAGE_TYPES[type-1], fullMessage);
		 break;
		 case "TPUT_UPDATE":
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
		 break;
		 case "PATH_UPDATE":
			//console.log(MESSAGE_TYPES[type-1], fullMessage);
			 Handle_UE_PATHUPDATE(fullMessage.payload);
		 break;
	 }
	 createAPI();
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
		if((API.type).toLowerCase() == apitype[0]){
			for(var vaps in API.vaps){
				var vapsinfo = API.vaps[vaps];
				overload24 = vapsinfo.overload;
				overload5 = vapsinfo.overload;
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
function createAPI() {
	for(var key in APinfo){
		var api_id = key;
		var API = APinfo[key];
		d3.selectAll('.image.' + api_id).remove();

		if((API.type).toLowerCase() == apitype[0]){
			createServingVap(api_id,routerconfig);
		}
		if((API.type).toLowerCase() == apitype[1]){
			createServingVap(api_id,extenderconfig);
		}
		drawAPI(api_id);
		for (var currVapKey in API.vaps) {
			var currVap = API.vaps[currVapKey];
			vaps[currVapKey] = currVap;
		}
	}
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
		if(checkifdatachange(ue_id,tputvalue)){
				tick(ue_id,tputvalue);
		}
	}
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
			updategauge('gauge5GHZ',value);
		break;
		case apiband[2]:
			updategauge('gauge5GHZ',value);
		break;
		}
	}
}
function Handle_UE_PATHUPDATE(message)
{
	for(var key in message){
		var ue_id = key;
		var band = message[key];
		if(band){
			var vap = vaps[band];
			band = vap.channel_group;
			if(band.indexOf('2')>-1)
				band=apiband[0];
			else
				band=apiband[1];
		}
		updateuepath(ue_id,band,APinfo,clientinfo);
	}
}
function includeJs(jsFilePath) {
    var js = document.createElement("script");

    js.type = "text/javascript";
    js.src = jsFilePath;

    document.body.appendChild(js);
}

