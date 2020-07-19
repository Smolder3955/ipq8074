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
		 type:config.type
	});
	APIobject.push(API);
}
function createUE(id,config,vaptype,channel_group,serving_vap)
{
	var x,y;
	switch(vaptype)
	{
		case apitype[0]:
			if(channel_group == apiband[0]){
				x = config.pos24ro.x;
				y = config.pos24ro.y;
			}
			if(channel_group == apiband[1]){
				x = config.pos5ro.x;
				y = config.pos5ro.y;
			}
		break;
		case apitype[1]:
			if(channel_group == apiband[0]){
				x = config.pos24ext.x;
				y = config.pos24ext.y;
			}
			if(channel_group == apiband[1]){
				x = config.pos5ext.x;
				y = config.pos5ext.y;
			}
		break;
		default:
			x = config.dx;
			y = config.dy;
		break;
	}
	var UE = new UEObject({
		 id:id,
		 config_id:config.config_id,
		 svg:d3.select("#Container"),
		 parent: document.body,
		 pos24ext:config.pos24ext,
		 pos5ext:config.pos5ext,
		 pos24ro:config.pos24ro,
		 pos5ro:config.pos5ro,
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
		 midline5band:config.midline5band,
		 position:config.position
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
	UE.plot();

}
function drawAPI(id)
{
	var API = findAPIobj(id);
	API.plot();
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
		var updatelevel = ue_signal_strength.indexOf('none');
		info.config.level = 'none';
		info.config.imagepath= "./resources/images/"+uecolor[clientorder]+"_"+info.config.type+"_signal_"+updatelevel.toString()+".png",
		createUE(key,info.config,'','','');
		drawUE(key);
		clientorder++;
	}
}

function updateue(ue_id,updateinfo,APinfo,clientinfo)
{	
	var UE = findUEobj(ue_id);
	if(UE){
		for(var key in clientinfo){
			if(ue_id == key){
				var info = clientinfo[key];
				var serving_vap = updateinfo.serving_vap;
				var clienttype = info.type;
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
				if(!updateinfo.signal_strength)
					updateinfo.signal_strength = ue_signal_strength[0];
				UE.vaptype = vaptype;
				var info = findchannelgroup(updateinfo);
				if(info)
					UE.updateUE(info);
				else
					UE.updateUE(null);
			}
		}
	}
}
function updateuepath(ue_id,band,APinfo,clientinfo)
{	
	var UE = findUEobj(ue_id);	
	if(UE){
		if(!band && UE.middle_channel_group){
			UE.removeMiddleline(true);
		}else{
			if(!UE.middle_channel_group){
				UE.updateMiddleUEPath(band);
			}else{
				if(UE.middle_channel_group != band){
					UE.removeMiddleline(false);
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
					updateinfo.signal_strength = updateinfo.signal_strength.toLowerCase();
					updateinfo.type = (API.type).toLowerCase();
					return updateinfo;
				}	
			}
		}
	}
}
