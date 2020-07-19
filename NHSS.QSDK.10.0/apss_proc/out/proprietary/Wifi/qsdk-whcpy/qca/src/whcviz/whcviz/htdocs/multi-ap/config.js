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
var title = ' | Wi-Fi Whole Home Coverage';
var powerby = 'powered by Qualcomm Atheros';
var apiband = ['CG_24G','CG_5GH','CG_5GL'];

var uecolor = ["purple","red","teal","yellow","blue","gray"]; 
var uelinecolor = ["#ce1288","#e2222f","#14a6b7","#fcb644","#44c8f1","gray"];  
 
var uetype = ["pc","tablet","phone","phone","phone","phone"];  
var ue_signal_strength = ['none','weak','low','moderate','high'];
var default_strength = ue_signal_strength.indexOf('none').toString();
var ymax = 300; //line chart y axis max value
var ping_duration = 250;
var xmax = 40;
var enablelinechart = true;

var gauge1config={
	x:120,
	y:350,
	size:155
}
var gauge2config={
	x:120,
	y:590,
	size:155
}

var routerconfig={
	x: 350,
	y: 330,
	width:381,
	height:244,
	imagepath:"./resources/images/ap_main.png",
	opacity:1,
	level: 0,
	color: 'none',
	type: apitype[0]
}              
var extenderconfig={
	x: 900,
	y: 335,
	width:244,
	height:244,
	imagepath:"./resources/images/ap_extender.png",
	opacity:1,
	level: 0,
	color: 'none',
	type: apitype[1]
} 
var ue1config = {
	config_id:'ue1config',
	pos24ext: {
		x: 1100,
		y: 220,
		line_x1: 1128,
		line_y1: 293,
		line_x2: 1085,
		line_y2: 355
	},
	pos5ext: {
		x: 1100,
		y: 600,
		line_x1: 1121,
		line_y1: 611,
		line_x2: 1085,
		line_y2: 559
	},
	pos24ro: {
		x: 340,
		y: 290,
		line_x1: 390,
		line_y1: 359,
		line_x2: 505,
		line_y2: 394
	},
	pos5ro: {
		x: 340,
		y: 530,
		line_x1: 401,
		line_y1: 546,
		line_x2: 503,
		line_y2: 510
	},
	midline24band:{
		x1: 920,
		y1: 390,
		x2: 715,
		y2: 390
	},             
	midline5band:{
		x1: 665,
		y1: 520,
		x2: 948,
		y2: 520
	},             
	width:75,
	height:75,
	opacity:1,
	dx: 800,
	dy: 145,
	position:30
};

// obj 2
var ue2config = {
	config_id:'ue2config',
	pos24ext: {
		x: 1210,
		y: 280,
		line_x1: 1226,
		line_y1: 346,
		line_x2: 1118,
		line_y2: 385
	},
	pos5ext: {
		x: 1210,
		y: 550,
		line_x1: 1218,
		line_y1: 578,
		line_x2: 1126,
		line_y2: 520
	},
	pos24ro: {
		x: 520,
		y: 200,
		line_x1: 558,
		line_y1: 271,
		line_x2: 578,
		line_y2: 336
	},
	pos5ro: {
		x: 520,
		y: 625,
		line_x1: 561,
		line_y1: 630,
		line_x2: 580,
		line_y2: 568
	},
	midline24band:{
		x1: 920,
		y1: 405,
		x2: 716,
		y2: 405
	},             
	midline5band:{
		x1: 665,
		y1: 505,
		x2: 948,
		y2: 505
	}, 
	width:75,
	height:75,
	opacity:1,
	dx: 950,
	dy: 145,
	position:10
}

// obj 3
var ue3config = {
	config_id:'ue3config',
	pos24ext: {
		x: 1255,
		y: 380,
		line_x1: 1259,
		line_y1: 420,
		line_x2: 1139,
		line_y2: 420
	},
	pos5ext: {
		x: 1255,
		y: 455,
		line_x1: 1261,
		line_y1: 490,
		line_x2: 1135,
		line_y2: 490
	},
	pos24ro: {
		x: 620,
		y: 220,
		line_x1: 654,
		line_y1: 293,
		line_x2: 640,
		line_y2: 339
	},
	pos5ro: {
		x: 620,
		y: 625,
		line_x1: 650,
		line_y1: 630,
		line_x2: 636,
		line_y2: 569
	},
	midline24band:{
		x1: 910,
		y1: 420,
		x2: 725,
		y2: 420
	},             
	midline5band:{
		x1: 665,
		y1: 490,
		x2: 948,
		y2: 490
	}, 
	width:75,
	height:75,
	opacity:1,
	dx: 1100,
	dy: 145,
	position:-20
}


var ue4config = {
	config_id:'ue4config',
	pos24ext: {
		x: 880,
		y: 250,
		line_x1: 921,
		line_y1: 324,
		line_x2: 961,
		line_y2: 354
	},
	pos5ext: {
		x: 880,
		y: 600,
		line_x1: 931,
		line_y1: 606,
		line_x2: 963,
		line_y2: 563
	},
	pos24ro: {
		x: 420,
		y: 220,
		line_x1: 470,
		line_y1: 291,
		line_x2: 529,
		line_y2: 364
	},
	pos5ro: {
		x: 420,
		y: 590,
		line_x1: 470,
		line_y1: 599,
		line_x2: 534,
		line_y2: 544
	},
	midline24band:{
		x1: 954,
		y1: 360,
		x2: 685,
		y2: 360
	},             
	midline5band:{
		x1: 665,
		y1: 550,
		x2: 948,
		y2: 550
	}, 
	width:75,
	height:75,
	opacity:1,
	dx: 1250,
	dy: 145,
	position:-50
}
var ue5config = {
	config_id:'ue5config',
	pos24ext: {
		x: 990,
		y: 220,
		line_x1: 1026,
		line_y1: 294,
		line_x2: 1026,
		line_y2: 339
	},
	pos5ext: {
		x: 990,
		y: 625,
		line_x1: 1026,
		line_y1: 629,
		line_x2: 1026,
		line_y2: 576
	},
	pos24ro: {
		x: 720,
		y: 250,
		line_x1: 748,
		line_y1: 323,
		line_x2: 688,
		line_y2: 358
	},
	pos5ro: {
		x: 720,
		y: 590,
		line_x1: 731,
		line_y1: 610,
		line_x2: 669,
		line_y2: 559
	},
	midline24band:{
		x1: 940,
		y1: 375,
		x2: 700,
		y2: 375
	},             
	midline5band:{
		x1: 665,
		y1: 535,
		x2: 948,
		y2: 535
	}, 
	width:75,
	height:75,
	opacity:1,
	dx: 1400,
	dy: 145,
	position:-70
}
var ue6config = {
	config_id:'ue6config',
	pos24ext: {
		x: 1475,
		y: 310,
		line_x1: 1498,
		line_y1: 374,
		line_x2: 1212,
		line_y2: 374
	},
	pos5ext: {
		x: 1475,
		y: 550,
		line_x1: 1498,
		line_y1: 434,
		line_x2: 1112,
		line_y2: 434
	},
	pos24ro: {
		x: 830,
		y: 160,
		line_x1: 862,
		line_y1: 235,
		line_x2: 767,
		line_y2: 316
	},
	pos5ro: {
		x: 830,
		y: 560,
		line_x1: 863,
		line_y1: 590,
		line_x2: 778,
		line_y2: 508
	},
	midline24band:{
		x1: 1040,
		y1: 316,
		x2: 771,
		y2: 316
	},             
	midline5band:{
		x1: 1040,
		y1: 494,
		x2: 771,
		y2: 494
	}, 
	width:75,
	height:75,
	opacity:1,
	dx: 1050,
	dy: 145,
	position:-30
}

var ueconfig = [ue1config,ue2config,ue3config,ue4config,ue5config];                     
                     
