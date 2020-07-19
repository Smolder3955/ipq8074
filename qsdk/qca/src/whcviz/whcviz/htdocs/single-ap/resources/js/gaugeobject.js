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
		define(['gaugeobject'], factory);
	} else if (typeof exports === 'object') {
		module.exports = factory(require('d3'));
	} else {
		root.GaugeObject= factory(root.d3);
	}
}(this, function(d3) {
	function GaugeObject(opts) {
		opts = opts || {};
		var gaugeobject = {
			id: opts.id,
			parent: opts.parent || 'body',
			cx: opts.cx || 300,
			cy: opts.cy || 400,
			svg: opts.svg || null,
			size: opts.size || 120,
			label: opts.label || '',
			min: opts.min || 0,
			max: opts.max || 100,
			level: opts.level || 0,
			majorTicks: opts.majorTicks || 2,
			minorTicks: opts.minorTicks || 2,
			textbandlabel: false,
			opacity: opts.opacity || 0,
			width: opts.width || 100,
			overload: opts.overload || 80,
			I:0,
			blueColor: opts.blueColor || "#88c540",
			yellowColor: opts.yellowColor || "#FF9900",
			redColor: opts.redColor || "#ed1c24",
			greyColor: opts.greyColor || "#cccccc",
			hidden: opts.hidden || false,
			transitionDuration:opts.transitionDuration || 500
		};
		gaugeobject.at = function(id) {
			gaugeobject.parent = id || 'body';
		}
		function createGauge(gaugeobject){

			gaugeobject.range = gaugeobject.max - gaugeobject.min;
	
			if (gaugeobject.level < 11){
				gaugeobject.redZones = [{ from: gaugeobject.min, to: gaugeobject.level }];
			}else if(10 < gaugeobject.level && gaugeobject.level < 26) {
				gaugeobject.yellowZones = [{ from: gaugeobject.min, to: gaugeobject.level }];
			}else{
				gaugeobject.greenZones = [{ from: gaugeobject.min, to: gaugeobject.level }];
			}
			gaugeobject.greyZones = [{ from: gaugeobject.min, to: gaugeobject.max }];
			
			gaugeobject.size = gaugeobject.size * 1.5;
			gaugeobject.raduis = gaugeobject.size * 0.97 / 3;
			
			gaugeobject.transitionDuration = 500;
			gaugeobject.meter = gaugeobject.svg.append("svg:circle")
                		.classed(gaugeobject.id, true)
						.attr("cx", gaugeobject.cx)
						.attr("cy", gaugeobject.cy)
						.attr("r", gaugeobject.raduis)
						.style("fill", "#2a3946")
						.style("opacity",.8)
						.style("stroke", "#2a3946")
						.style("stroke-width", "0.5px");
					
			gaugeobject.svg.append("svg:circle")
                		.classed(gaugeobject.id, true)
						.attr("cx", gaugeobject.cx)
						.attr("cy", gaugeobject.cy)
						.attr("r", 0.9 * gaugeobject.raduis)
						.style("fill", "#425868")
						.style("stroke", "#425868")
						.style("opacity",.8)
						.style("stroke-width", "2px");

			gaugeobject.render();
			gaugeobject.redraw(gaugeobject.level);
	
		}
		gaugeobject.draw = function() {
			createGauge(gaugeobject);
		}
		gaugeobject.updateGauges = function(theNumber) {
			if (theNumber > gaugeobject.overload){
				gaugeobject.drawBand(0,theNumber, gaugeobject.redColor);
			}else if(10 < theNumber && theNumber < 26) {
				gaugeobject.drawBand(0, theNumber, "#88c540");
			}else{
				gaugeobject.drawBand(0, theNumber, "#88c540");
			}
			gaugeobject.drawRestBand(0, 100, "#708090");
			gaugeobject.redraw(theNumber);
		}
		gaugeobject.render = function(value) {
		  
			for (var index in gaugeobject.greyZones)
			{
				gaugeobject.drawRestBand(gaugeobject.greyZones[index].from, gaugeobject.greyZones[index].to, gaugeobject.greyColor);
			}
		
		
			for (var index in gaugeobject.greenZones)
			{
				gaugeobject.drawBand(gaugeobject.greenZones[index].from, gaugeobject.greenZones[index].to, gaugeobject.blueColor);
			}
		
			for (var index in gaugeobject.yellowZones)
			{
				gaugeobject.drawBand(gaugeobject.yellowZones[index].from, gaugeobject.yellowZones[index].to, gaugeobject.yellowColor);
			}
		
			for (var index in gaugeobject.redZones)
			{
				gaugeobject.drawBand(gaugeobject.redZones[index].from, gaugeobject.redZones[index].to, gaugeobject.redColor);
			}
		
	   
			if (undefined != gaugeobject.label)
			{
				/*var fontSize = Math.round(gaugeobject.size / 12);
				gaugeobject.svg.append("svg:text")
				.attr("x", gaugeobject.cx)
				.attr("y", gaugeobject.cy - gaugeobject.size/2+fontSize)
				.attr("dy", fontSize / 2)
				.attr("text-anchor", "middle")
				.text(gaugeobject.label)
				.style("font-size", fontSize + "px")
				.style("fill", "#333")
				.style("stroke-width", "0px");*/
			}
			if(gaugeobject.textbandlabel){        
				var fontSize = Math.round(gaugeobject.size / 16);
				var majorDelta = gaugeobject.range / (gaugeobject.majorTicks - 1);
				for (var major = gaugeobject.min; major <= gaugeobject.max; major += majorDelta)
				{
					var minorDelta = majorDelta / gaugeobject.minorTicks;
					for (var minor = major + minorDelta; minor < Math.min(major + majorDelta, gaugeobject.max); minor += minorDelta)
					{
						var point1 = gaugeobject.valueToPoint(minor, 0.75);
						var point2 = gaugeobject.valueToPoint(minor, 0.85);
				
						gaugeobject.svg.append("svg:line")
						.attr("x1", point1.x)
						.attr("y1", point1.y)
						.attr("x2", point2.x)
						.attr("y2", point2.y)
						.style("stroke", "#666")
						.style("stroke-width", "0px");
					}
			
					var point1 = gaugeobject.valueToPoint(major, 0.6);
					var point2 = gaugeobject.valueToPoint(major, 0.9);	
			
					gaugeobject.svg.append("svg:line")
					.attr("x1", point1.x)
					.attr("y1", point1.y)
					.attr("x2", point2.x)
					.attr("y2", point2.y)
					.style("stroke", "#fff")
					.style("stroke-width", "6px");
			
					if ( major == gaugeobject.max)
					{
						var point = gaugeobject.valueToPoint(major, 0.82);
				
						gaugeobject.svg.append("svg:text")
						.attr("x", point.x - 4)
						.attr("y", point.y + 12)
						.attr("dy", fontSize / 3)
						.attr("text-anchor", major == gaugeobject.min ? "start" : "end")
						.text("")
						.style("font-size", (fontSize + 5 ) + "px")
						.style("fill", "#333")
						.style("stroke-width", "0px")
						.style("font-weight", "bold");
					}
					if (major == gaugeobject.min )
					{
						var point = gaugeobject.valueToPoint(major, 0.82);
				
						gaugeobject.svg.append("svg:text")
						.attr("x", point.x + 2)
						.attr("y", point.y + 12)
						.attr("dy", fontSize / 3)
						.attr("text-anchor", major == gaugeobject.min ? "start" : "end")
						.text("")
						.style("font-size", (fontSize + 5 ) + "px")
						.style("fill", "#333")
						.style("stroke-width", "0px")
						.style("font-weight", "bold");
					}
				}
			}
					
			gaugeobject.pointerContainer = gaugeobject.svg.append("svg:g").attr("class", "pointerContainer");
		
			var midValue = (gaugeobject.min + gaugeobject.max) / 2;
		
			gaugeobject.pointerPath = gaugeobject.buildPointerPath(midValue);
		
			var pointerLine = d3.svg.line()
			.x(function(d) { return d.x })
			.y(function(d) { return d.y })
			.interpolate("basis");
		
			gaugeobject.pointerContainer.selectAll("path")
			.data([gaugeobject.pointerPath])
			.enter()
			.append("svg:path")
			.attr("d", pointerLine)
			.style("fill", "#000000")
			.style("stroke", "#000000")
			.style("fill-opacity", 1)
		
			gaugeobject.pointerContainer.append("svg:circle")
			.attr("cx", gaugeobject.cx)
			.attr("cy", gaugeobject.cy)
			.attr("r", 0.09 * gaugeobject.raduis)
			.style("fill", "#000000")
			.style("stroke", "#000000")
			.style("opacity", 1);

			var fontSize = Math.round(gaugeobject.size / 10);
			gaugeobject.pointerContainer.selectAll("text")
			.data([midValue])
			.enter()
			.append("svg:text")
			.attr("x", gaugeobject.cx )
			.attr("y", gaugeobject.cy + 37)
			.attr("dy", fontSize / 2)
			.attr("text-anchor", "middle")
			.style("font-size", 40 + "px")
			.style("font","Arial Bold")
			.style("fill", "#FFF")
			.style("stroke-width", "0px");
			gaugeobject.redraw(gaugeobject.min, 0);
		}
		gaugeobject.drawBand = function(start, end, color) {
			if (0 >= end - start) return; 
			if(!gaugeobject.bands)    
				gaugeobject.bands = gaugeobject.svg.append("svg:path").attr("class", 'bands');
			var theend = gaugeobject.valueToRadians(end);
			gaugeobject.arc = d3.svg.arc()
			.innerRadius(0.35 * gaugeobject.raduis)
			.outerRadius(0.85 * gaugeobject.raduis)
			.startAngle(0);
			gaugeobject.bandsdraw = gaugeobject.bands.attr("transform", function() { return "translate(" + gaugeobject.cx + ", " + gaugeobject.cy + ") rotate(270)" });
		
		
			if ( gaugeobject.I === 0){
				gaugeobject.arcs = gaugeobject.bandsdraw
				.datum({endAngle: 0 })
				.style("fill", color)
				.attr("d",gaugeobject.arc); 
			}else{
				gaugeobject.arcs = gaugeobject.bandsdraw
				.datum({endAngle: gaugeobject.valueToRadians(gaugeobject.I) })
				.style("fill", color)
				.attr("d",gaugeobject.arc);
			}
		
			gaugeobject.arcs.transition().duration(500).call(arcTween, end);
		
			function arcTween(transition, newAngle) {
				transition.attrTween("d", function(d) {
					var theAngle = (newAngle / 100 * 180 - (0 / 100 * 180 )) * Math.PI / 180;
					var interpolate = d3.interpolate(d.endAngle, theAngle);
					return function(t) {
						d.endAngle = interpolate(t);
						gaugeobject.I = newAngle;
						return gaugeobject.arc(d);
					};
				});
			}
		}
		gaugeobject.buildPointerPath = function(value) {
			var delta = gaugeobject.range / 13;
		
			var head = valueToPoint(value, 0.6);
			var head1 = valueToPoint(value - delta, 0.12);
			var head2 = valueToPoint(value + delta, 0.12);
		
			var tailValue = value - (gaugeobject.range * (1/(180/360)) / 2);
			var tail = valueToPoint(tailValue, -.1);
			var tail1 = valueToPoint(tailValue - delta, 0.12);
			var tail2 = valueToPoint(tailValue + delta, 0.12);
		
			return [head, head1, tail2, tail, tail1, head2, head];
		
			function valueToPoint(value, factor)
			{
				var point = gaugeobject.valueToPoint(value, factor);
				point.x -= gaugeobject.cx;
				point.y -= gaugeobject.cy;
				return point;
			}
		}
		gaugeobject.drawRestBand = function(start, end, color) {		
			if (0 >= end - start) return; 
			if(!gaugeobject.restbands)
				gaugeobject.restbands = gaugeobject.svg.append("svg:path").attr("class", 'backbands');
			gaugeobject.theendrestbands = gaugeobject.valueToRadians(end);
			gaugeobject.bandsdrawn = gaugeobject.restbands.attr("transform", function() { return "translate(" + gaugeobject.cx + ", " + gaugeobject.cy + ") rotate(270)" });
			gaugeobject.arcEnd = d3.svg.arc()
			.innerRadius(0.35 * gaugeobject.raduis)
			.outerRadius(0.85 * gaugeobject.raduis)
			.startAngle(gaugeobject.valueToRadians(start))
			.endAngle(gaugeobject.valueToRadians(end));
			gaugeobject.arcss = gaugeobject.bandsdrawn
			.style("fill", color)
			.style("opacity",.3)
			.attr("d",gaugeobject.arcEnd);         
		}
		gaugeobject.redraw = function(value, transitionDuration) {
		
			gaugeobject.pointerContainer.selectAll("text").text(Math.round(value) + '%');
		
			var pointer = gaugeobject.pointerContainer.selectAll("path");
			pointer.transition()
			.duration(undefined != transitionDuration ? transitionDuration : gaugeobject.transitionDuration)
			.attrTween("transform", function()
			   {
				   var pointerValue = value;
				   if (value > gaugeobject.max) pointerValue = gaugeobject.max + 0.02*gaugeobject.range;
				   else if (value < gaugeobject.min) pointerValue = gaugeobject.min - 0.02*gaugeobject.range;
				   var targetRotation = (gaugeobject.valueToDegrees(pointerValue) - 90);
				   var currentRotation = gaugeobject._currentRotation || targetRotation;
				   gaugeobject._currentRotation = targetRotation;
		   
				   return function(step) 
				   {
					   var rotation = currentRotation + (targetRotation-currentRotation)*step;
					   return "translate(" + gaugeobject.cx + ", " + gaugeobject.cy + ") rotate(" + rotation + ")"; 
				   }
			   });  
		}
		gaugeobject.valueToDegrees = function(value) {
        	return value / gaugeobject.range * 180 - (gaugeobject.min / gaugeobject.range * 180 );
		}
		gaugeobject.valueToRadians = function(value) {
        	return gaugeobject.valueToDegrees(value) * Math.PI / 180;
		}
		gaugeobject.valueToPoint = function(value, factor) {
			return { 	x: gaugeobject.cx - gaugeobject.raduis * factor * Math.cos(gaugeobject.valueToRadians(value)),
						y: gaugeobject.cy - gaugeobject.raduis * factor * Math.sin(gaugeobject.valueToRadians(value)) 		
					};
		}
		return gaugeobject;
	}
	return GaugeObject;
}));

