/**
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All rights reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

//event_ap,event_band,event_path
var n = xmax,
	duration = 750;
var t =0;
var datax = [];
var w = 1600,
	h = 142
var opacityvalue;
if(enablelinechart)
	opacityvalue = 1;
else
	opacityvalue = 0;
	
var margin = {top: 1, right: 1, bottom: 1, left: 50},
	width = w - margin.left - margin.right,
	height = h - margin.top - margin.bottom;

var x = d3.scale.linear()
	.domain([t-n+1, t])
	.range([0, width]);

var y = d3.time.scale()
	.range([height, 0])
	.domain([0, ymax]);;

var line = d3.svg.line()
	.x(function (d, i) {return x(d.time);})
	.y(function (d, i) {return y(d.value);});

var svg = d3.select('#chart').append('svg')
	.attr('class', 'chart')
	.attr("style", "outline: thin solid grey;") 
	.attr("width", w)
	.attr("height", h)
	.attr("viewBox", "0 0 " + w + " " + h );
	
var g = svg.append("g")
	.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

var graph = g.append("svg")
	.attr("width", width)
	.attr("height", height + margin.top + margin.bottom);	

var axis = graph.append('g')
	.attr('class', 'x axis')
	.attr('transform', 'translate(0,' + height + ')')
	.call(x.axis = d3.svg.axis().scale(x).ticks(0).orient('bottom'))

g.append("g")
	.attr("class", "y axis")
	.call(d3.svg.axis().scale(y).ticks(0).tickSize(0, 0).orient("left"))
	.append("text")
		.attr("transform", "rotate(-90)")
		.attr("x", -75)
		.attr("y",-30)
		.attr("dy", ".71em")
		.style("text-anchor", "middle")
		.style("font-family","Arial")
		.style("font-size","17px")
		.style("fill", "#333333")
		.text("EVENTS");
var paths = graph.append('g')

function init() {
	for (var name in clientinfo) {
		var group = clientinfo[name];
		group.path = paths.append('path')
			.data([group.data])
			.style("stroke-opacity", opacityvalue)
			.attr('class', name + ' group')
			.style('stroke', group.config.linecolor)
	}
}

function tick(ue_id,tputvalue) {
	for (var name in clientinfo) {
		var group = clientinfo[name];
		var length = group.data.length;
		break;
	}
	t++;
	// Add new values
	if(length == n-2){
		x.domain([t - n + 2,t ]);
	}else{
		x.domain([1,40 ]);
	}
	for (var name in clientinfo) {
		var group = clientinfo[name];
		if(name == ue_id){
			group.data.push({time: t, value: tputvalue});
		}else{
			var length = group.data.length;
			if(length)
				group.data.push({time: t, value: group.data[length-1].value});
			else
				group.data.push({time: t, value: 0});
		}
		group.path.attr('d', line);
	}
	// Slide paths left
	if(length == n-2){
		paths.attr('transform', null)
			.transition()
			.style("stroke-opacity", opacityvalue)
			.duration(duration)
			.ease('linear')
			.attr("transform", "translate(" + x(t-n+1) + ")");
	}else{
		paths.attr('transform', null)
			.transition()
			.style("stroke-opacity", opacityvalue)
			.duration(duration)
			.ease('linear')
			.attr("transform", "translate(" + x(1) + ")");
	}
	var marktime;
	var currentendpoint;
	if(datax.length){
		marktime = datax[datax.length-1].time;
		currentendpoint = translateAlong(group.path.node(),marktime);
	}
	var ticks = graph.selectAll('line')
		.data(datax);

	ticks.enter().append('line')
			.attr('x1', currentendpoint)
			.attr('x2', currentendpoint)
			.attr('y1', 0)
			.attr('y2', height)
			.attr('class', 'line1');
	if(length == n-2){
		ticks.transition()
			.ease('linear')
			.duration(duration)
			.attr('x1', function(d) {
				return x(d.time);
			})
			.attr('x2', function(d) {
				return x(d.time);
			})
			.attr('y1', 0)
			.attr('y2', height)
	}
	ticks.exit().remove();
	
	var image = graph.selectAll('image')
		.data(datax);
	image.enter().append('image')
		.attr("x",  currentendpoint-19)
		.attr('y',function(d) {
			return height/2+d.position;
		})
		.attr('width',38)
		.attr('height',38)
		.attr("xlink:href", function(d) { 
			var imagename = pickimage(d.uename,d.eventtype);
			return "./resources/images/"+imagename;
		});
	if(length == n-2){
		image.transition()
			.ease('linear')
			.duration(duration)
			.attr('x', function(d, i){
				if (!isNaN(x(d.time))){
					return x(d.time)-19;
				}
			})
	}

	image.exit().remove();

    if(length == n-2){
		// Remove oldest data point from each group
		for (var name in clientinfo) {
			var group = clientinfo[name]
			group.data.shift()
		}
	}
}

function translateAlong(path,t) {
  var l = path.getTotalLength();
      var p = path.getPointAtLength(t * l);
      return p.x;
}

function updatetick(uename,eventtype,position) {
	var obj = {
		time: t,
		uename: uename,
		eventtype:eventtype,
		position:position
	};
	datax.push(obj);
}
function pickimage(uename,eventtype) {
	for (var name in clientinfo) {
		if(name == uename){
			var group = clientinfo[name];
			var color = group.config.color;
			break;
		}
	}
	return imagename = eventtype+'_'+color+'.png';
}
