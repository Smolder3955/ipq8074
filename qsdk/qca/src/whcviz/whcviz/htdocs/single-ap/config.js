/**
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All rights reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

var MESSAGE_TYPES = ["FULL_UPDATE", "TEXT", "TPUT_UPDATE",
                     "ASSOCIATION_UPDATE", "STEERING_UPDATE",
                     "UTIL_UPDATE", "BLACKOUT_UPDATE",
                     "OVERLOAD_THRESH", "OVERLOAD_UPDATE",
                     "STEERING_ATTEMPT", "AP_INFO",
                     "FLAG_UPDATE","PATH_UPDATE","CLIENT_INFO"];
var clientarray =[];
var apitype = ['cap','re'];
var titleimage = './resources/images/titleimage.png';
var footnote = 'Qualcomm Wi-Fi SON is a product of Qualcomm Atheros, Inc.';
var apiband = ['CG_24G','CG_5GH','CG_5GL'];

var uecolor = ["blue","orange","pink","purple","red","yellow"]; 
var uelinecolor = ["#44c8f1","#ea7600","#ce1288","#7a4183","#e2222f","fcb644"];  
 
var uetype = ["pc","tablet","phone","phone","phone","phone"];  
var ue_signal_strength = ['call','data','movie'];
var default_strength = ue_signal_strength.indexOf('call').toString();

var routerconfig={
	x: 50,
	y: 397,
	width:267,
	height:269,
	imagepath:"./resources/images/api.png",
	opacity:1,
	level: 0,
	color: 'none',
	type: apitype[0]
}              

var gauge1config={
	x:1500,
	y:750,
	size:155
}
var gauge2config={
	x:1500,
	y:530,
	size:155
}
var gauge3config={
	x:1500,
	y:310,
	size:155
}

var traffic_config = {
	interactive: "call",
	streaming: "movie",
	download: "data",
	other: "call"
}

var imgW = 135;
var imgH = 135;
 
var ue1config = {
	config_id:'ue1config',
	pos5high: {
		x: 430,
		y: 225,
	},
	pos5low: {
		x: 480,
		y: 440,
	},
	pos24: {
		x: 460,
		y: 660,
	},           
	width:imgW,
	height:imgH,
	opacity:1,
	dx: 480,
	dy: 145,
	bw:0
};

// obj 2
var ue2config = {
	config_id:'ue2config',
	pos5high: {
		x: 590,
		y: 225,
	},
	pos5low: {
		x: 640,
		y: 440,
	},
	pos24: {
		x: 620,
		y: 660,
	},    
	width:imgW,
	height:imgH,
	opacity:1,
	dx: 640,
	dy: 145,
	bw:0
}

// obj 3
var ue3config = {
	config_id:'ue3config',
	pos5high: {
		x: 740,
		y: 225,
	},
	pos5low: {
		x: 800,
		y: 440,
	},
	pos24: {
		x: 780,
		y: 660,
	},    
	width:imgW,
	height:imgH,
	opacity:1,
	dx: 800,
	dy: 145,
	bw:0
}


var ue4config = {
	config_id:'ue4config',
	pos5high: {
		x: 890,
		y: 225,
	},
	pos5low: {
		x: 960,
		y: 440,
	},
	pos24: {
		x: 940,
		y: 660,
	},    
	width:imgW,
	height:imgH,
	opacity:1,
	dx: 960,
	dy: 145,
	bw:0
}
var ue5config = {
	config_id:'ue5config',
	pos5high: {
		x: 1040,
		y: 225,
	},
	pos5low: {
		x: 1120,
		y: 440,
	},
	pos24: {
		x: 1100,
		y: 660,
	},    
	width:imgW,
	height:imgH,
	opacity:1,
	dx: 1120,
	dy: 145,
	bw:0
}
var ue6config = {
	config_id:'ue6config',
	pos5high: {
		x: 1190,
		y: 225,
	},
	pos5low: {
		x: 1280,
		y: 440,
	},
	pos24: {
		x: 1260,
		y: 660,
	},    
	width:imgW,
	height:imgH,
	opacity:1,
	dx: 1280,
	dy: 145,
	bw:0
}

var ueconfig = [ue1config,ue2config,ue3config,ue4config,ue5config,ue6config];                     
                     
