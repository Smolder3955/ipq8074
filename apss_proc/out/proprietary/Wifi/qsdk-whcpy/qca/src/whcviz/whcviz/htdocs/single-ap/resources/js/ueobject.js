/**
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
 * All rights reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

var BANDWIDTH_COLOR_NO_POLLUTION = "#FFFFFF";
var BANDWIDTH_COLOR_POLLUTION = "#FF0000";

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
			pos24:opts.pos24,
			//pos5ext:opts.pos5ext,
			pos5high:opts.pos5high,
			pos5low:opts.pos5low,
		 	midline24band:opts.midline24band,
		 	midline5band:opts.midline5band,
			channel_group:opts.channel_group,
			middle_channel_group:opts.middle_channel_group,
			vaptype:opts.vaptype,
			serving_vap:opts.serving_vap,
			middlepath:'',
			totalbw:opts.serving_vap,
			uebw:opts.uebw,
			firstpass:opts.firstpass,
			polluted:false
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

			ueobject.firstpass = true;

		}

		function mouse(d,i) {
			console.log(d3.mouse(this));
		}

		ueobject.plot = function() {
			creatdevice(ueobject);
		}

		ueobject.updateUE = function(updateinfo) {
			var servingapp = updateinfo.serving_vap;
			if(servingapp){
				//steer = false;
				var steer = updateinfo.was_steered;
				if(ueobject.serving_vap != updateinfo.serving_vap){
					ueobject.ping(updateinfo,steer);
					ueobject.serving_vap = updateinfo.serving_vap;
					ueobject.polluted = updateinfo.polluted;
				}
				else{

				}
			}else{

			}
		}
		ueobject.updatePolluted = function(new_polluted) {
			if(new_polluted != ueobject.polluted){
				ueobject.polluted = new_polluted;
				ueobject.drawpolluted(new_polluted);
			}
		}
		ueobject.updateMiddleUEPath = function(channel_group) {
			switch(channel_group)
			{
				case apiband[0]:
					if(ueobject.middle_channel_group) {
						ueobject.pingMiddleLine5(channel_group);
					}
					else {
						ueobject.drawMiddle24line(channel_group);
					}
				break;
				case apiband[1]:
					if(ueobject.middle_channel_group) {
						ueobject.pingMiddleLine24(channel_group);
					}
					else {
						ueobject.drawMiddle5line(channel_group);
					}
				break;
			}
		}
		ueobject.moveUE = function(updateinfo) {
			var x_move;
			var y_move;
			switch(updateinfo.channel_group)
			{
				case apiband[0]:
					x_move = ueobject.pos24.x;
					y_move = ueobject.pos24.y;
				break;
				case apiband[1]:
					x_move = ueobject.pos5high.x;
					y_move = ueobject.pos5high.y;
				break;
				case apiband[2]:
					x_move = ueobject.pos5low.x;
					y_move = ueobject.pos5low.y;
				break;
			}

			//d3.selectAll('.text.' + ueobject.id).attr("opacity",0);
			d3.selectAll('.image.' + ueobject.id)
					.transition()
					.attr("x",x_move)
					.attr("y",y_move)
					.attr("opacity",1)
					.duration(1000)
					.each("end",function() {
						//d3.selectAll('.text.' + ueobject.id).attr("opacity",1);
						ueobject.vaptype = updateinfo.type;
						ueobject.x=x_move;
						ueobject.y=y_move;
						ueobject.channel_group=updateinfo.channel_group;
						ueobject.drawConnectionLine(updateinfo);
						//updatetick(ueobject.id,'event_ap',0);
			})
			d3.selectAll('.text.' + ueobject.id)
					//.text('')
					.transition()
					.attr("x",x_move+ueobject.width/2)
					.attr("y",y_move+ueobject.height/2 +85)
					.attr("opacity",1)
					.duration(1000)
					.each("end",function() {
						d3.selectAll('.text.' + ueobject.id)
							.text(ueobject.uebw + ' Mbps');
					})
		}
		ueobject.moveUEhidden = function(updateinfo) {
			var x_move;
			var y_move;
			switch(updateinfo.channel_group)
			{
				case apiband[0]:
					x_move = ueobject.pos24.x;
					y_move = ueobject.pos24.y;
				break;
				case apiband[1]:
					x_move = ueobject.pos5high.x;
					y_move = ueobject.pos5high.y;
				break;
				case apiband[2]:
					x_move = ueobject.pos5low.x;
					y_move = ueobject.pos5low.y;
				break;
			}

			d3.selectAll('.text.' + ueobject.id).attr("opacity",0);
			ueobject.device.attr("opacity",0);
			d3.selectAll('.image.' + ueobject.id)
					.transition()
					.attr("x",x_move)
					.attr("y",y_move)
					.attr("opacity",1)
					.duration(1000)
					.each("end",function() {
						ueobject.device.attr("opacity",1);
						d3.selectAll('.text.' + ueobject.id).attr("opacity",1);
						ueobject.vaptype = updateinfo.type;
						ueobject.x=x_move;
						ueobject.y=y_move;
						ueobject.channel_group=updateinfo.channel_group;
						ueobject.drawConnectionLine(updateinfo);
						//updatetick(ueobject.id,'event_ap',0);
			})

			d3.selectAll('.text.' + ueobject.id)
					.transition()
					.attr("x",x_move+ueobject.width/2)
					.attr("y",y_move+ueobject.height/2 +85)
					.duration(1000)
					.each("end",function() {
						d3.selectAll('.text.' + ueobject.id)
							.text(ueobject.uebw + ' Mbps');
					})
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
					   });
			ueobject.middle_channel_group = channel_group;
		}
		ueobject.removeMiddleline = function() {
			d3.selectAll('.line.' + ueobject.id+'_lowband').remove();
			d3.selectAll('.line.' + ueobject.id+'_highband').remove();
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
						.attr("x1", ueobject.pos5high.line_x1)
						.attr("y1", ueobject.pos5high.line_y1)
						.attr("x2", ueobject.pos5high.line_x1)
						.attr("y2", ueobject.pos5high.line_y1)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
							.duration(1500)
							.attr({
							  x2: ueobject.pos5high.line_x2,
							  y2: ueobject.pos5high.line_y2
							})
					   .each("end",function() {
					   });
			//console.log('UELINE',ueobject);
  		}
		ueobject.drawCAP5line = function() {
			 ueobject.svg.append("line")
			 			.attr("class", "line")
                		.classed(ueobject.id, true)
						.attr("x1", ueobject.pos5low.line_x1)
						.attr("y1", ueobject.pos5low.line_y1)
						.attr("x2", ueobject.pos5low.line_x1)
						.attr("y2", ueobject.pos5low.line_y1)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
							.duration(1500)
							.attr({
							  x2: ueobject.pos5low.line_x2,
							  y2: ueobject.pos5low.line_y2
							})
					   .each("end",function() {
					   });
		}
		ueobject.drawEXT24line = function() {
			 ueobject.svg.append("line")
			 			.attr("class", "line")
                		.classed(ueobject.id, true)
						.attr("x1", ueobject.pos24.line_x1)
						.attr("y1", ueobject.pos24.line_y1)
						.attr("x2", ueobject.pos24.line_x1)
						.attr("y2", ueobject.pos24.line_y1)
						.attr("stroke-width", 5)
						.attr("opacity",  ueobject.opacity)
						.attr("stroke",  ueobject.linecolor)
						.transition()
							.duration(1500)
							.attr({
							  x2: ueobject.pos24.line_x2,
							  y2: ueobject.pos24.line_y2
							})
					   .each("end",function() {
					   });
			//console.log('UELINE',ueobject);
		}
		ueobject.drawEXT5line = function() {
			/* ueobject.svg.append("line")
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
					   });*/
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
			   .attr("width",ue1config.width)
			   .attr("height",ue1config.height)
			   .attr("x",ueobject.x)
			   .attr("y",ueobject.y)
			   .style("opacity",1)
			   .each("end",function() {
					ueobject.device.transition()
					.duration(200)
					.attr("width",185)
					.attr("height",185)
					   .attr("x",ueobject.x-25)
					   .attr("y",ueobject.y-25)
					.style("opacity",1)
					.each("end",function() {
						 ueobject.device.transition()
						 .duration(200)
						 .attr("width",ue1config.width)
						 .attr("height",ue1config.height)
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
        	.attr("cy", ueobject.y+60)
        	.attr("cx", ueobject.x+65)
        	.attr("r", 37.5)
        	.attr("stroke", ueobject.linecolor)
        	.attr("opacity", 1)
        	.attr("fill-opacity", 0)
        	.attr("stroke-width", 4)
        	.transition()
        	.duration(500)
    		.attr("r",80)
        	.attr("opacity",0)
        	.each("end",function() {
        		ueobject.svg.append("circle")
						.attr("cy", ueobject.y+60)
						.attr("cx", ueobject.x+65)
						.attr("r", 37.5)
						.attr("stroke", ueobject.linecolor)
						.attr("opacity", 1)
						.attr("fill-opacity", 0)
						.attr("stroke-width", 4)
						.transition()
						.duration(500)
						.attr("r",80)
						.attr("opacity",0)
						.each("end",function() {
        					ueobject.svg.append("circle")
								.attr("cy", ueobject.y+60)
								.attr("cx", ueobject.x+65)
								.attr("r", 37.5)
								.attr("stroke", ueobject.linecolor)
								.attr("opacity", 1)
								.attr("fill-opacity", 0)
								.attr("stroke-width", 4)
								.transition()
								.duration(500)
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
			ueobject.svg.append("line")
        	.attr("x1", ueobject.midline24band.x1)
			.attr("y1", ueobject.midline24band.y1)
			.attr("x2", ueobject.midline24band.x2)
			.attr("y2", ueobject.midline24band.y2)
			.attr("stroke-width", 5)
			.attr("opacity",  ueobject.opacity)
			.attr("stroke",  ueobject.linecolor)
        	.transition()
        	.duration(500)
			.attr("x1", ueobject.midline24band.x1)
			.attr("y1", ueobject.midline24band.y1+10)
			.attr("x2", ueobject.midline24band.x2)
			.attr("y2", ueobject.midline24band.y2+10)
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
						.duration(500)
						.attr("x1", ueobject.midline24band.x1)
						.attr("y1", ueobject.midline24band.y1+10)
						.attr("x2", ueobject.midline24band.x2)
						.attr("y2", ueobject.midline24band.y2+10)
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
								.duration(500)
								.attr("x1", ueobject.midline24band.x1)
								.attr("y1", ueobject.midline24band.y1+10)
								.attr("x2", ueobject.midline24band.x2)
								.attr("y2", ueobject.midline24band.y2+10)
								.attr("opacity",0)
								.each("end",function() {
									ueobject.drawMiddle5line(channel_group);
									//updatetick(ueobject.id,'event_path',20);
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
        	.duration(500)
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
						.duration(500)
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
								.duration(500)
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
			ueobject.svg.append("line")
        	.attr("x1", ueobject.midline5band.x1)
			.attr("y1", ueobject.midline5band.y1)
			.attr("x2", ueobject.midline5band.x2)
			.attr("y2", ueobject.midline5band.y2)
			.attr("stroke-width", 5)
			.attr("opacity",  ueobject.opacity)
			.attr("stroke",  ueobject.linecolor)
        	.transition()
        	.duration(500)
			.attr("x1", ueobject.midline5band.x1)
			.attr("y1", ueobject.midline5band.y1+10)
			.attr("x2", ueobject.midline5band.x2)
			.attr("y2", ueobject.midline5band.y2+10)
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
						.duration(500)
						.attr("x1", ueobject.midline5band.x1)
						.attr("y1", ueobject.midline5band.y1+10)
						.attr("x2", ueobject.midline5band.x2)
						.attr("y2", ueobject.midline5band.y2+10)
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
								.duration(500)
								.attr("x1", ueobject.midline5band.x1)
								.attr("y1", ueobject.midline5band.y1+10)
								.attr("x2", ueobject.midline5band.x2)
								.attr("y2", ueobject.midline5band.y2+10)
								.attr("opacity",0)
								.each("end",function() {
									ueobject.drawMiddle24line(channel_group);
									//updatetick(ueobject.id,'event_path',-40);
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
        	.duration(500)
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
						.duration(500)
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
								.duration(500)
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
		ueobject.drawband = function(text) {
				if(ueobject.firstpass) {
					 ueobject.svg.append("text")
						 .attr("class", "text")
						 .classed(ueobject.id, true)
						 .attr("x", ueobject.x+ueobject.width/2)
						 .attr("y", ueobject.y+ueobject.height+20)
						 .attr("dy", ".35em")
						 .attr("font-size",20)
						 .attr("fill",'#FFFFFF')
						 .attr("font-family","arial")
						 .attr("text-anchor","middle")
						 .attr("font-weight","bold")
						 .text(text + ' Mbps');

					ueobject.firstpass = false;
				}
				else {
					 d3.selectAll('.text.' + ueobject.id)
						 .text(text + ' Mbps');
				}
		}
		ueobject.drawtotalband = function(text,stringval) {
			d3.selectAll('.texttotal.' + ueobject.id).remove();
			ueobject.svg.append("text")
				.attr("class", "texttotal")
				.classed(ueobject.id, true)
				.attr("x", ueobject.x+ueobject.width/2)
				.attr("y", ueobject.y+ueobject.height-75)
				.attr("dy", ".35em")
				.attr("font-size",35)
				.attr("fill",'#FFFFFF')
				.attr("font-family","arial")
				.attr("text-anchor","middle")
				.text(text)

			ueobject.svg.append("text")
				.attr("class", "texttotal")
				.classed(ueobject.id, true)
				.attr("x", ueobject.x+ueobject.width/2)
				.attr("y", ueobject.y+ueobject.height-40)
				.attr("dy", ".35em")
				.attr("font-size",25)
				.attr("fill",'#FFFFFF')
				.attr("font-family","arial")
				.attr("text-anchor","middle")
				.text('Mbps');
		}

		ueobject.drawpolluted = function(polluted) {
			d3.selectAll('.text.' + ueobject.id)
				.attr("fill",polluted ? BANDWIDTH_COLOR_POLLUTION :
				                        BANDWIDTH_COLOR_NO_POLLUTION);
                }

                return ueobject;
	}
	return UEObject;
}));

