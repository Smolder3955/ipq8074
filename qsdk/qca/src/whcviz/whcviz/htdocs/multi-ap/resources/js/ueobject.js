/**
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All rights reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

(function (root, factory) {
	if (typeof define === 'function' && define.amd) {
		define(['ueobject'], factory);
	} else if (typeof exports === 'object') {
		module.exports = factory(require('d3'));
	} else {
		root.UEObject= factory(root.d3);
	}
}(this, function(d3) {
	function UEObject(opts) {
		opts = opts || {};
		var ueobject = {
			id: opts.id,
			config_id:opts.config_id,
			parent: opts.parent || 'body',
			x: opts.x || 300,
			y: opts.y || 400,
			svg: opts.svg || null,
			opacity: opts.opacity || 0,
			width: opts.width || 100,
			height: opts.height || 100,
			imagepath: opts.imagepath || "./resources/images/dev1.png",
			hidden: opts.hidden || false,
			status: opts.status || "off",
			level: opts.level || ue_signal_strength[0],
			color: opts.color || uecolor[0],
			linecolor: opts.linecolor || uelinecolor[0],
			type: opts.type || uetype[0],
			pos24ext:opts.pos24ext,
			pos5ext:opts.pos5ext,
			pos24ro:opts.pos24ro,
			pos5ro:opts.pos5ro,
		 	midline24band:opts.midline24band,
		 	midline5band:opts.midline5band,
			channel_group:opts.channel_group,
			middle_channel_group:opts.middle_channel_group,
			vaptype:opts.vaptype,
			serving_vap:opts.serving_vap,
			middlepath:'',
			position:opts.position || 0,

		};
		ueobject.at = function(id) {
			ueobject.parent = id || 'body';
		}
		function creatdevice(ueobject){
			ueobject.device = ueobject.svg.append("svg:image")
				.attr("class", "image")
                .classed(ueobject.id, true)
				.attr('x',ueobject.x)
				.attr('y',ueobject.y)
				.attr('width', ueobject.width)
				.attr('height', ueobject.height)
				.attr('opacity',ueobject.opacity)
				.attr("xlink:href",ueobject.imagepath)
				.on('click',mouse);
			if(ueobject.type != apitype[0] && ueobject.type != apitype[1])
				ueobject.fadeon();
				
		}
		
		function mouse(d,i) {
			console.log(d3.mouse(this));
		}
		
		ueobject.plot = function() {
			creatdevice(ueobject);
		}
		ueobject.updateUE = function(updateinfo) {
			if(updateinfo) {
				 var servingapp = updateinfo.serving_vap;
				 if(servingapp){
					 var steer = updateinfo.was_steered;
					 if(ueobject.serving_vap != updateinfo.serving_vap){
						 ueobject.updateUESignal(updateinfo);
						 ueobject.ping(updateinfo,steer);
					
						 if(ueobject.vaptype == apitype[0])
							 ueobject.removeMiddleline(true);
						
						 ueobject.serving_vap = updateinfo.serving_vap;
						
					 }
					 else{
						 ueobject.updateUESignal(updateinfo);
						 if(ueobject.serving_vap != updateinfo.serving_vap)
							 ueobject.drawConnectionLine(updateinfo);
					 }
				 }else{
				 	d3.selectAll('.line.' + ueobject.id).remove();
				 }
			}
			else {
				d3.selectAll('.line.' + ueobject.id).remove();
			}
		}
		ueobject.updateMiddleUEPath = function(channel_group) {
			switch(channel_group)
			{
				case apiband[0]:
					ueobject.drawMiddle24line(channel_group);
				break;
				case apiband[1]:
					ueobject.drawMiddle5line(channel_group);
				break;
			}
		}
		ueobject.moveUE = function(updateinfo) {
			var x_move;
			var y_move;
			switch(updateinfo.channel_group)
			{
				case apiband[0]:
					if(updateinfo.type == apitype[0]){
						x_move = ueobject.pos24ro.x;
						y_move = ueobject.pos24ro.y;
					}
					if(updateinfo.type == apitype[1]){
						x_move = ueobject.pos24ext.x;
						y_move = ueobject.pos24ext.y;
					}
				break;
				case apiband[1]:
					if(updateinfo.type == apitype[0]){
						x_move = ueobject.pos5ro.x;
						y_move = ueobject.pos5ro.y;
					}
					if(updateinfo.type == apitype[1]){
						x_move= ueobject.pos5ext.x;
						y_move  = ueobject.pos5ext.y;
					}
				break;
			}
			d3.selectAll('.line.' + ueobject.id).remove();
			if(ueobject.channel_group == updateinfo.channel_group){
			// router <----> extender
				d3.selectAll('.image.' + ueobject.id)
					.transition()
					.attr("x",x_move)
					.attr("y",y_move)
					.attr("opacity",1)
					.duration(1000)
					.each("end",function() {
						ueobject.vaptype = updateinfo.type;
						ueobject.x=x_move;
						ueobject.y=y_move;
						ueobject.channel_group=updateinfo.channel_group;
						ueobject.drawConnectionLine(updateinfo);
						updatetick(ueobject.id,'event_ap',ueobject.position);
					})
			}else{
				if(ueobject.vaptype !=updateinfo.type){
					if(ueobject.channel_group == apiband[0]){
					//if the band = 24 then move down to 5
						d3.selectAll('.image.' + ueobject.id)
							.transition()
							.attr("x",x_move)
							.attr("y",ueobject.y)
							.attr("opacity",1)
							.duration(200)
							.each("end",function() {
								d3.selectAll('.image.' + ueobject.id)
									.transition()
									.attr("x",x_move)
									.attr("y",y_move)
									.attr("opacity",1)
									.duration(1000)
									.each("end",function() {
										ueobject.vaptype = updateinfo.type;
										ueobject.x=x_move;
										ueobject.y=y_move;
										ueobject.channel_group=updateinfo.channel_group;
										ueobject.drawConnectionLine(updateinfo);
										updatetick(ueobject.id,'event_band',ueobject.position);
									})
							})
					}
					if(ueobject.channel_group == apiband[1]){
					//if the band = 5 then move up to 24
						d3.selectAll('.image.' + ueobject.id)
							.transition()
							.attr("x",ueobject.x)
							.attr("y",y_move)
							.attr("opacity",1)
							.duration(200)
							.each("end",function() {
								d3.selectAll('.image.' + ueobject.id)
									.transition()
									.attr("x",x_move)
									.attr("y",y_move)
									.attr("opacity",1)
									.duration(1000)
									.each("end",function() {
										ueobject.vaptype = updateinfo.type;
										ueobject.x=x_move;
										ueobject.y=y_move;
										ueobject.channel_group=updateinfo.channel_group;
										ueobject.drawConnectionLine(updateinfo);
										updatetick(ueobject.id,'event_band',ueobject.position);
									})
							})
					}
				}else{
					d3.selectAll('.image.' + ueobject.id)
						.transition()
						.attr("x",x_move)
						.attr("y",y_move)
						.attr("opacity",1)
						.duration(1000)
						.each("end",function() {
							ueobject.vaptype = updateinfo.type;
							ueobject.x=x_move;
							ueobject.y=y_move;
							ueobject.channel_group=updateinfo.channel_group;
							ueobject.drawConnectionLine(updateinfo);
							updatetick(ueobject.id,'event_band',ueobject.position);
						})
				}
			}
		}
		ueobject.moveUEhidden = function(updateinfo) {
			var x_move;
			var y_move;
			switch(updateinfo.channel_group)
			{
				case apiband[0]:
					if(updateinfo.type == apitype[0]){
						x_move = ueobject.pos24ro.x;
						y_move = ueobject.pos24ro.y;
					}
					if(updateinfo.type == apitype[1]){
						x_move = ueobject.pos24ext.x;
						y_move = ueobject.pos24ext.y;
					}
				break;
				case apiband[1]:
					if(updateinfo.type == apitype[0]){
						x_move = ueobject.pos5ro.x;
						y_move = ueobject.pos5ro.y;
					}
					if(updateinfo.type == apitype[1]){
						x_move= ueobject.pos5ext.x;
						y_move  = ueobject.pos5ext.y;
					}
				break;
			}
			d3.selectAll('.line.' + ueobject.id).remove();
			if(ueobject.channel_group == updateinfo.channel_group){
				ueobject.device.attr("opacity",0);
			// router <----> extender
				d3.selectAll('.image.' + ueobject.id)
					.transition()
					.attr("x",x_move)
					.attr("y",y_move)
					.duration(1000)
					.each("end",function() {
						ueobject.device.attr("opacity",1);
						ueobject.vaptype = updateinfo.type;
						ueobject.x=x_move;
						ueobject.y=y_move;
						ueobject.channel_group=updateinfo.channel_group;
						ueobject.drawConnectionLine(updateinfo);
					})
			}else{
				if(ueobject.vaptype !=updateinfo.type){
					if(ueobject.channel_group == apiband[0]){
						ueobject.device.attr("opacity",0);
					//if the band = 24 then move down to 5
						d3.selectAll('.image.' + ueobject.id)
							.transition()
							.attr("x",x_move)
							.attr("y",ueobject.y)
							.duration(200)
							.each("end",function() {
								d3.selectAll('.image.' + ueobject.id)
									.transition()
									.attr("x",x_move)
									.attr("y",y_move)
									.duration(1000)
									.each("end",function() {
										ueobject.device.attr("opacity",1);
										ueobject.vaptype = updateinfo.type;
										ueobject.x=x_move;
										ueobject.y=y_move;
										ueobject.channel_group=updateinfo.channel_group;
										ueobject.drawConnectionLine(updateinfo);
									})
							})
					}
					if(ueobject.channel_group == apiband[1]){
						ueobject.device.attr("opacity",0);
					//if the band = 5 then move down to 24
						d3.selectAll('.image.' + ueobject.id)
							.transition()
							.attr("x",ueobject.x)
							.attr("y",y_move)
							.duration(200)
							.each("end",function() {
								d3.selectAll('.image.' + ueobject.id)
									.transition()
									.attr("x",x_move)
									.attr("y",y_move)
									.duration(1000)
									.each("end",function() {
										ueobject.device.attr("opacity",1);
										ueobject.vaptype = updateinfo.type;
										ueobject.x=x_move;
										ueobject.y=y_move;
										ueobject.channel_group=updateinfo.channel_group;
										ueobject.drawConnectionLine(updateinfo);
									})
							})
					}
				}else{
					ueobject.device.attr("opacity",0);
					d3.selectAll('.image.' + ueobject.id)
						.transition()
						.attr("x",x_move)
						.attr("y",y_move)
						.duration(1000)
						.each("end",function() {
							ueobject.device.attr("opacity",1);
							ueobject.vaptype = updateinfo.type;
							ueobject.x=x_move;
							ueobject.y=y_move;
							ueobject.channel_group=updateinfo.channel_group;
							ueobject.drawConnectionLine(updateinfo);
						})
				}
			}
		}
		
		ueobject.updateUESignal = function(updateinfo) {
			var newlevel = ue_signal_strength.indexOf(updateinfo.signal_strength);
			var currentlevel = ue_signal_strength.indexOf(ueobject.level);
			var path = ueobject.imagepath;
			var newpath = path.replace(currentlevel.toString(), newlevel.toString());
			//console.log('newpath',newpath);	
			ueobject.imagepath = newpath;
			ueobject.level = updateinfo.signal_strength;
			d3.select('.image.' + ueobject.id)
    			.attr("xlink:href", newpath);
		}
		ueobject.drawConnectionLine = function(updateinfo) {
			var serving_vap = updateinfo.serving_vap;
			switch(ueobject.vaptype)
			{
				case apitype[0]:
					ueobject.drawCAPline();
				break;
				case apitype[1]:
					ueobject.drawEXTline();
				break;
			}
		}

		ueobject.drawMiddle24line = function(channel_group) {
			if(ueobject.middle_channel_group)
				d3.selectAll('.line.' + ueobject.id+'_highband').remove();
				
			ueobject.svg.append("line")
						.attr("class", "line")
						.classed(ueobject.id+'_lowband', true)
						.attr("x1", ueobject.midline24band.x2)
						.attr("y1", ueobject.midline24band.y2)
						.attr("x2", ueobject.midline24band.x2)
						.attr("y2", ueobject.midline24band.y2)
						.attr("stroke-width", 5)
						.attr("opacity", ueobject.opacity)
						.attr("stroke", ueobject.linecolor)
						.transition()
							.duration(1500)
							.attr({
							  x2: ueobject.midline24band.x1,
							  y2: ueobject.midline24band.y1
							})
					   .each("end",function() {
							updatetick(ueobject.id,'event_path',ueobject.position);
							createAPI();
					   });
			ueobject.middle_channel_group = channel_group;
		}
		ueobject.drawMiddle5line = function(channel_group) {
			if(ueobject.middle_channel_group)
				d3.selectAll('.line.' + ueobject.id+'_lowband').remove();
			ueobject.svg.append("line")
						.attr("class", "line")
						.classed(ueobject.id+'_highband', true)
						.attr("x1", ueobject.midline5band.x1)
						.attr("y1", ueobject.midline5band.y1)
						.attr("x2", ueobject.midline5band.x1)
						.attr("y2", ueobject.midline5band.y1)
						.attr("stroke-width", 5)
						.attr("opacity", ueobject.opacity)
						.attr("stroke", ueobject.linecolor)
						.transition()
							.duration(1500)
							.attr({
							  x2: ueobject.midline5band.x2,
							  y2: ueobject.midline5band.y2
							})
					   .each("end",function() {
							updatetick(ueobject.id,'event_path',ueobject.position);
							createAPI();
					   });
			ueobject.middle_channel_group = channel_group;
		}
		ueobject.removeMiddleline = function(reset) {
			d3.selectAll('.line.' + ueobject.id+'_lowband').remove();
			d3.selectAll('.line.' + ueobject.id+'_highband').remove();
			
			if(reset)
				ueobject.middle_channel_group = null;
			
		}
		ueobject.drawCAPline = function() {
			switch(ueobject.channel_group)
			{
				case apiband[0]:
					ueobject.drawCAP24line();
				break;
				case apiband[1]:
					ueobject.drawCAP5line();
				break;
			}
		}
		ueobject.drawEXTline = function() {
			switch(ueobject.channel_group)
			{
				case apiband[0]:
					ueobject.drawEXT24line();
				break;
				case apiband[1]:
					ueobject.drawEXT5line();
				break;
			}
		}

		ueobject.drawCAP24line = function() {
			 var line = ueobject.svg.append("line")
			 			.attr("class", "line")
                		.classed(ueobject.id, true)
						.attr("x1", ueobject.pos24ro.line_x1)
						.attr("y1", ueobject.pos24ro.line_y1)
						.attr("x2", ueobject.pos24ro.line_x1)
						.attr("y2", ueobject.pos24ro.line_y1)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
							.duration(1500)
							.attr({
							  x2: ueobject.pos24ro.line_x2,
							  y2: ueobject.pos24ro.line_y2
							})
					   .each("end",function() {
							createAPI();
					   });
			//console.log('UELINE',ueobject);
  		}
		ueobject.drawCAP5line = function() {
			 ueobject.svg.append("line")
			 			.attr("class", "line")
                		.classed(ueobject.id, true)
						.attr("x1", ueobject.pos5ro.line_x1)
						.attr("y1", ueobject.pos5ro.line_y1)
						.attr("x2", ueobject.pos5ro.line_x1)
						.attr("y2", ueobject.pos5ro.line_y1)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
							.duration(1500)
							.attr({
							  x2: ueobject.pos5ro.line_x2,
							  y2: ueobject.pos5ro.line_y2
							})
					   .each("end",function() {
							createAPI();
					   });
		}
		ueobject.drawEXT24line = function() {
			 ueobject.svg.append("line")
			 			.attr("class", "line")
                		.classed(ueobject.id, true)
						.attr("x1", ueobject.pos24ext.line_x1)
						.attr("y1", ueobject.pos24ext.line_y1)
						.attr("x2", ueobject.pos24ext.line_x1)
						.attr("y2", ueobject.pos24ext.line_y1)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
							.duration(1500)
							.attr({
							  x2: ueobject.pos24ext.line_x2,
							  y2: ueobject.pos24ext.line_y2
							})
					   .each("end",function() {
							createAPI();
					   });
			//console.log('UELINE',ueobject);
		}
		ueobject.drawEXT5line = function() {
			 ueobject.svg.append("line")
			 			.attr("class", "line")
                		.classed(ueobject.id, true)
						.attr("x1", ueobject.pos5ext.line_x1)
						.attr("y1", ueobject.pos5ext.line_y1)
						.attr("x2", ueobject.pos5ext.line_x1)
						.attr("y2", ueobject.pos5ext.line_y1)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
							.duration(1500)
							.attr({
							  x2: ueobject.pos5ext.line_x2,
							  y2: ueobject.pos5ext.line_y2
							})
					   .each("end",function() {
							createAPI();
					   });
					
		}

		ueobject.fadeoff = function() {
			ueobject.device.transition()
				.delay(3000)
			   .style("opacity",0)
			   .duration(3000) 
			   .each("end",function() {
			   		ueobject.setStatus("off");
			   })
		}
		ueobject.fadeon = function() {
			ueobject.device.transition()
			   .duration(200)
			   .attr("width",75)
			   .attr("height",75)
			   .attr("x",ueobject.x)
			   .attr("y",ueobject.y)
			   .style("opacity",1)
			   .each("end",function() {
					ueobject.device.transition()
					.duration(200)
					.attr("width",125)
					.attr("height",125)
					   .attr("x",ueobject.x-25)
					   .attr("y",ueobject.y-25)
					.style("opacity",1)
					.each("end",function() {
						 ueobject.device.transition()
						 .duration(200)
						 .attr("width",75)
						 .attr("height",75)
					   .attr("x",ueobject.x)
					   .attr("y",ueobject.y)
						 .each("end",function() {
							 ueobject.setStatus("off");
						 })
					}) 
			   })
		}
		
		ueobject.ping = function(updateinfo,steer) {			
			ueobject.svg.append("circle")
        	.attr("cy", ueobject.y+37.5)
        	.attr("cx", ueobject.x+37.5)
        	.attr("r", 37.5)
        	.attr("stroke", ueobject.linecolor)
        	.attr("opacity", 1)
        	.attr("fill-opacity", 0)
        	.attr("stroke-width", 4)
        	.transition()
        	.duration(ping_duration)
    		.attr("r",80)
        	.attr("opacity",0)
        	.each("end",function() {
        		ueobject.svg.append("circle")
						.attr("cy", ueobject.y+37.5)
						.attr("cx", ueobject.x+37.5)
						.attr("r", 37.5)
						.attr("stroke", ueobject.linecolor)
						.attr("opacity", 1)
						.attr("fill-opacity", 0)
						.attr("stroke-width", 4)
						.transition()
						.duration(ping_duration)
						.attr("r",80)
						.attr("opacity",0)
						.each("end",function() {
        					ueobject.svg.append("circle")
								.attr("cy", ueobject.y+37.5)
								.attr("cx", ueobject.x+37.5)
								.attr("r", 37.5)
								.attr("stroke", ueobject.linecolor)
								.attr("opacity", 1)
								.attr("fill-opacity", 0)
								.attr("stroke-width", 4)
								.transition()
								.duration(ping_duration)
								.attr("r",80)
								.attr("opacity",0)
								.each("end",function() {
									if(steer)
										ueobject.moveUE(updateinfo);
									else 
										ueobject.moveUEhidden(updateinfo);
								})
								.remove()
        				})
        				.remove();
        	})
        	.remove();
		}
		
		
		ueobject.pingMiddleLine24 = function(channel_group) {		
			createAPI();	
			ueobject.svg.append("line")
        	.attr("x1", ueobject.midline24band.x1)
			.attr("y1", ueobject.midline24band.y1)
			.attr("x2", ueobject.midline24band.x2)
			.attr("y2", ueobject.midline24band.y2)
			.attr("stroke-width", 5)
			.attr("opacity",  ueobject.opacity)
			.attr("stroke",  ueobject.linecolor)
        	.transition()
        	.duration(ping_duration)
			.attr("x1", ueobject.midline24band.x1)
			.attr("y1", ueobject.midline24band.y1+10)
			.attr("x2", ueobject.midline24band.x2)
			.attr("y2", ueobject.midline24band.y2+10)
        	.attr("opacity",0)
        	.each("end",function() {
        		createAPI();
        		ueobject.svg.append("line")
						.attr("x1", ueobject.midline24band.x1)
						.attr("y1", ueobject.midline24band.y1)
						.attr("x2", ueobject.midline24band.x2)
						.attr("y2", ueobject.midline24band.y2)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
						.duration(ping_duration)
						.attr("x1", ueobject.midline24band.x1)
						.attr("y1", ueobject.midline24band.y1+10)
						.attr("x2", ueobject.midline24band.x2)
						.attr("y2", ueobject.midline24band.y2+10)
						.attr("opacity",0)
						.each("end",function() {
							createAPI();
        					ueobject.svg.append("line")
								.attr("x1", ueobject.midline24band.x1)
								.attr("y1", ueobject.midline24band.y1)
								.attr("x2", ueobject.midline24band.x2)
								.attr("y2", ueobject.midline24band.y2)
								.attr("stroke-width", 5)
								.attr("opacity",  ueobject.opacity)
								.attr("stroke",  ueobject.linecolor)
								.transition()
								.duration(ping_duration)
								.attr("x1", ueobject.midline24band.x1)
								.attr("y1", ueobject.midline24band.y1+10)
								.attr("x2", ueobject.midline24band.x2)
								.attr("y2", ueobject.midline24band.y2+10)
								.attr("opacity",0)
								.each("end",function() {
									createAPI();
									ueobject.drawMiddle5line(channel_group);
									updatetick(ueobject.id,'event_path',ueobject.position);
								})
								.remove()
        				})
        				.remove();
        	})
        	.remove();
        	
        	
        	ueobject.svg.append("line")
        	.attr("x1", ueobject.midline24band.x1)
			.attr("y1", ueobject.midline24band.y1)
			.attr("x2", ueobject.midline24band.x2)
			.attr("y2", ueobject.midline24band.y2)
			.attr("stroke-width", 5)
			.attr("opacity",  ueobject.opacity)
			.attr("stroke",  ueobject.linecolor)
        	.transition()
        	.duration(ping_duration)
			.attr("x1", ueobject.midline24band.x1)
			.attr("y1", ueobject.midline24band.y1-10)
			.attr("x2", ueobject.midline24band.x2)
			.attr("y2", ueobject.midline24band.y2-10)
        	.attr("opacity",0)
        	.each("end",function() {
        		ueobject.svg.append("line")
						.attr("x1", ueobject.midline24band.x1)
						.attr("y1", ueobject.midline24band.y1)
						.attr("x2", ueobject.midline24band.x2)
						.attr("y2", ueobject.midline24band.y2)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
						.duration(ping_duration)
						.attr("x1", ueobject.midline24band.x1)
						.attr("y1", ueobject.midline24band.y1-10)
						.attr("x2", ueobject.midline24band.x2)
						.attr("y2", ueobject.midline24band.y2-10)
						.attr("opacity",0)
						.each("end",function() {
        					ueobject.svg.append("line")
								.attr("x1", ueobject.midline24band.x1)
								.attr("y1", ueobject.midline24band.y1)
								.attr("x2", ueobject.midline24band.x2)
								.attr("y2", ueobject.midline24band.y2)
								.attr("stroke-width", 5)
								.attr("opacity",  ueobject.opacity)
								.attr("stroke",  ueobject.linecolor)
								.transition()
								.duration(ping_duration)
								.attr("x1", ueobject.midline24band.x1)
								.attr("y1", ueobject.midline24band.y1-10)
								.attr("x2", ueobject.midline24band.x2)
								.attr("y2", ueobject.midline24band.y2-10)
								.attr("opacity",0)
								.each("end",function() {
									//ueobject.moveUE(updateinfo);
								})
								.remove()
        				})
        				.remove();
        	})
        	.remove();
		}
		ueobject.pingMiddleLine5 = function(channel_group) {
			createAPI();			
			ueobject.svg.append("line")
        	.attr("x1", ueobject.midline5band.x1)
			.attr("y1", ueobject.midline5band.y1)
			.attr("x2", ueobject.midline5band.x2)
			.attr("y2", ueobject.midline5band.y2)
			.attr("stroke-width", 5)
			.attr("opacity",  ueobject.opacity)
			.attr("stroke",  ueobject.linecolor)
        	.transition()
        	.duration(ping_duration)
			.attr("x1", ueobject.midline5band.x1)
			.attr("y1", ueobject.midline5band.y1+10)
			.attr("x2", ueobject.midline5band.x2)
			.attr("y2", ueobject.midline5band.y2+10)
        	.attr("opacity",0)
        	.each("end",function() {
        		createAPI();
        		ueobject.svg.append("line")
						.attr("x1", ueobject.midline5band.x1)
						.attr("y1", ueobject.midline5band.y1)
						.attr("x2", ueobject.midline5band.x2)
						.attr("y2", ueobject.midline5band.y2)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
						.duration(ping_duration)
						.attr("x1", ueobject.midline5band.x1)
						.attr("y1", ueobject.midline5band.y1+10)
						.attr("x2", ueobject.midline5band.x2)
						.attr("y2", ueobject.midline5band.y2+10)
						.attr("opacity",0)
						.each("end",function() {
							createAPI();
        					ueobject.svg.append("line")
								.attr("x1", ueobject.midline5band.x1)
								.attr("y1", ueobject.midline5band.y1)
								.attr("x2", ueobject.midline5band.x2)
								.attr("y2", ueobject.midline5band.y2)
								.attr("stroke-width", 5)
								.attr("opacity",  ueobject.opacity)
								.attr("stroke",  ueobject.linecolor)
								.transition()
								.duration(ping_duration)
								.attr("x1", ueobject.midline5band.x1)
								.attr("y1", ueobject.midline5band.y1+10)
								.attr("x2", ueobject.midline5band.x2)
								.attr("y2", ueobject.midline5band.y2+10)
								.attr("opacity",0)
								.each("end",function() {
									createAPI();
									ueobject.drawMiddle24line(channel_group);
									updatetick(ueobject.id,'event_path',ueobject.position);
								})
								.remove()
        				})
        				.remove();
        	})
        	.remove();
        	
        	
        	ueobject.svg.append("line")
        	.attr("x1", ueobject.midline5band.x1)
			.attr("y1", ueobject.midline5band.y1)
			.attr("x2", ueobject.midline5band.x2)
			.attr("y2", ueobject.midline5band.y2)
			.attr("stroke-width", 5)
			.attr("opacity",  ueobject.opacity)
			.attr("stroke",  ueobject.linecolor)
        	.transition()
        	.duration(ping_duration)
			.attr("x1", ueobject.midline5band.x1)
			.attr("y1", ueobject.midline5band.y1-10)
			.attr("x2", ueobject.midline5band.x2)
			.attr("y2", ueobject.midline5band.y2-10)
        	.attr("opacity",0)
        	.each("end",function() {
        		ueobject.svg.append("line")
						.attr("x1", ueobject.midline5band.x1)
						.attr("y1", ueobject.midline5band.y1)
						.attr("x2", ueobject.midline5band.x2)
						.attr("y2", ueobject.midline5band.y2)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
						.duration(ping_duration)
						.attr("x1", ueobject.midline5band.x1)
						.attr("y1", ueobject.midline5band.y1-10)
						.attr("x2", ueobject.midline5band.x2)
						.attr("y2", ueobject.midline5band.y2-10)
						.attr("opacity",0)
						.each("end",function() {
        					ueobject.svg.append("line")
								.attr("x1", ueobject.midline5band.x1)
								.attr("y1", ueobject.midline5band.y1)
								.attr("x2", ueobject.midline5band.x2)
								.attr("y2", ueobject.midline5band.y2)
								.attr("stroke-width", 5)
								.attr("opacity",  ueobject.opacity)
								.attr("stroke",  ueobject.linecolor)
								.transition()
								.duration(ping_duration)
								.attr("x1", ueobject.midline5band.x1)
								.attr("y1", ueobject.midline5band.y1-10)
								.attr("x2", ueobject.midline5band.x2)
								.attr("y2", ueobject.midline5band.y2-10)
								.attr("opacity",0)
								.each("end",function() {
									//ueobject.moveUE(updateinfo);
								})
								.remove()
        				})
        				.remove();
        	})
        	.remove();
		}
		ueobject.checkStatus = function() {
			return ueobject.status;
		}
		ueobject.setStatus = function(status) {
			ueobject.status = status;
		}
		ueobject.remove = function() {
			d3.selectAll('.line.' + ueobject.id).remove();
			d3.selectAll('.image.' + ueobject.id).remove();
        }
        		return ueobject;
	}
	return UEObject;
}));

