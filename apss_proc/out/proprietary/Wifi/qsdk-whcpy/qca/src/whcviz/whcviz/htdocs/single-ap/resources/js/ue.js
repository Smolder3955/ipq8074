/**
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All rights reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

var UEobject = [];
var APIobject = [];
function createServingVap(id,config)
{
	var API = new UEObject({
		 id:id,
		 svg:d3.select("#Container"),
		 parent: document.body,
		 x: config.x,
		 y: config.y,
		 width: config.width,
		 height: config.height,
		 imagepath: config.imagepath,
		 opacity:config.opacity,
		 level:config.level,
		 color:config.color,
		 type:config.type,
		 opacity:0
	});
	APIobject.push(API);
}
function createUE(id,config,vaptype,channel_group,serving_vap)
{
	var x,y;
	/*switch(channel_group)
	{
		case apitype[0]:
			if(channel_group == apiband[0]){
				x = config.pos5high.x;
				y = config.pos5high.y;
			}
			if(channel_group == apiband[1]){
				x = config.pos5low.x;
				y = config.pos5low.y;
			}
		break;
		case apitype[1]:
			if(channel_group == apiband[0]){
				x = config.pos24.x;
				y = config.pos24.y;
			}
			if(channel_group == apiband[1]){

			}
		break;
		default:
			x = config.dx;
			y = config.dy;
		break;
	}*/
	x = config.dx;
	y = config.dy;

	var UE = new UEObject({
		 id:id,
		 config_id:config.config_id,
		 svg:d3.select("#Container"),
		 parent: document.body,
		 pos24:config.pos24,
		 //pos5ext:config.pos5ext,
		 pos5high:config.pos5high,
		 pos5low:config.pos5low,
		 x: x,
		 y: y,
		 width: config.width,
		 height: config.height,
		 opacity: config.opacity,
		 imagepath: config.imagepath,
		 status:config.status,
		 level:config.level,
		 color:config.color,
		 linecolor:config.linecolor,
		 type:config.type,
		 channel_group:channel_group,
		 vaptype:vaptype,
		 serving_vap:'',
		 serving_vap:'',
		 midline24band:config.midline24band,
		 midline5band:config.midline5band
	});
	UEobject.push(UE);
}
function findUEobj(id)
{
	for(var i = 0; i<UEobject.length; i++)
	{
		if(UEobject[i].id === id)
			return UEobject[i];
	}
}
function removeUEobj(id)
{
	for(var i = 0; i<UEobject.length; i++)
	{
		if(UEobject[i].id === id){
			d3.selectAll(id).remove();
			UEobject.splice(i,1);
		}
	}
}
function findAPIobj(id)
{
	for(var i = 0; i<APIobject.length; i++)
	{
		if(APIobject[i].id === id)
			return APIobject[i];
	}
}
function drawUE(id)
{
	var UE = findUEobj(id);
	//console.log('UE',UE);
	UE.plot();

}
function drawAPI(id)
{
	var API = findAPIobj(id);
	API.plot();
	API.drawtotalband('0');
}
function createdefaultue()
{
	var clientorder = 0;
	for(var key in clientinfo){
		var info = clientinfo[key];
		var clienttype = info.type;
		if(clienttype.toLowerCase() == 'laptop')
			clienttype = 'pc';
		info.config.type = clienttype.toLowerCase();
		var trafficClass = "";
		if(info.traffic_class == "STREAMING")
			trafficClass = traffic_config.streaming;
		else if(info.traffic_class == "INTERACTIVE")
			trafficClass = traffic_config.interactive;
		else if(info.traffic_class == "DOWNLOAD")
			trafficClass = traffic_config.download;
		else
			trafficClass = traffic_config.other;
		info.config.imagepath= "./resources/images/"+uecolor[clientorder]+"_"+info.config.type+"_"+trafficClass.toString()+".png",
		createUE(key,info.config,'','','');
		drawUE(key);
		updatebandtext(key,'0');
		clientorder++;
	}
}
function updatebandtext(ue_id,text)
{
	var UE = findUEobj(ue_id);
	if(UE){
		UE.uebw = parseInt(text);
		UE.drawband(text);
	}
}

function updateuembps(ue_id,tputvalue)
{
	if(tputvalue != null)
		updatebandtext(ue_id,tputvalue);
	else
		updatebandtext(ue_id,0);
}

function updatetputue(ue_id,info) {

}

function updateue(ue_id,updateinfo,APinfo,clientinfo)
{
	var UE = findUEobj(ue_id);
	if(UE){
		for(var key in clientinfo){
			if(ue_id == key){
				var info = clientinfo[key];
				var serving_vap = updateinfo.serving_vap;
				//var clienttype = info.type;
				for(var vap_id in APinfo){
					var vapinfo = APinfo[vap_id];
					for(var vap_name in vapinfo.vaps){
						api_vap_info = vapinfo.vaps[vap_name];
						if(vap_name == serving_vap){
							var vaptype = vapinfo.type.toLowerCase();
							var channel_group = api_vap_info.channel_group;
							break;
						}
					}
				}
				//if(!updateinfo.signal_strength)
				//	updateinfo.signal_strength = 'weak';
				UE.vaptype = vaptype;

				//UE.channel_group = channel_group;
				var info = findchannelgroup(updateinfo);
				if(info)
					UE.updateUE(info);
			}
		}
	}
}
function updateueflag(ue_id,updateinfo,APinfo,clientinfo)
{
	var UE = findUEobj(ue_id);
	if(UE){
		UE.updatePolluted(updateinfo.polluted)
	}
}
function updateuepath(ue_id,band,APinfo,clientinfo)
{
	console.log('ue_id',ue_id);
	console.log('band',band);
	var UE = findUEobj(ue_id);
	if(UE){
		console.log('UE.middle_channel_group',UE.middle_channel_group);
		if(!band && UE.middle_channel_group){
			UE.removeMiddleline();
		}else{
			if(!UE.middle_channel_group){
				UE.updateMiddleUEPath(band);
			}else{
				if(UE.middle_channel_group != band){
					//console.log('channel_group',channel_group);
					UE.removeMiddleline();
					UE.updateMiddleUEPath(band);
				}
			}
		}
	}
}
function findchannel(api_type,vap_id)
{
	for(var key in APinfo){
		var api_id = key;
		var API = APinfo[key];
		if(api_type == key){
			for(var vap_name in API.vaps){
				api_vap_info = API.vaps[vap_name];
				if(vap_id == vap_name){
					var channel_group = api_vap_info.channel_group;
					return channel_group;
				}
			}
		}
	}

}
function findchannelgroup(updateinfo)
{
	vap_id = updateinfo.serving_vap;
	if(vap_id){
		for(var key in APinfo){
			var api_id = key;
			var API = APinfo[key];
			for(var vap_name in API.vaps){
				api_vap_info = API.vaps[vap_name];
				if(vap_id == vap_name){
					var channel_group = api_vap_info.channel_group;
					updateinfo.channel_group= channel_group;
					updateinfo.apiname = api_id;
					//updateinfo.signal_strength = updateinfo.signal_strength.toLowerCase();
					//updateinfo.type = (API.type).toLowerCase();
					return updateinfo;
				}
			}
		}
	}
}

function addtotalbw() {
	var api = APIobject[0];
	var total = 0;
	var totstring = '';
	for(var i = 0; i<UEobject.length; i++)
	{
		total += parseInt(UEobject[i].uebw);
		totstring += UEobject[i].uebw.toString() + "+";
	}

	totstring = totstring.substring(0,totstring.length-1);
	totstring = totstring+ "="+total;
	api.totalbw = parseInt(total);
	api.drawtotalband(parseInt(total),totstring);
}
