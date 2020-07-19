/**
 * @@-COPYRIGHT-START-@@
 *
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All rights reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * @@-COPYRIGHT-END-@@
 */

var gaugeobj = [];
function creategauge(id,x,y,size,label,overload)
{
	var gauge = new GaugeObject({
		 id:id,
		 svg:d3.select("#Container"),
		 parent: document.body,
		 cx: x,
		 cy: y,
		 opacity: 0,
		 size:size,
		 level:0,
		 label:label,
		 overload:overload
	});
	gauge.draw();
	gaugeobj.push(gauge);
}
function updategauge(id,value)
{	
	var gauge = findobj(id);
	gauge.updateGauges(value);
}
function findobj(id)
{
	for(var i = 0; i<gaugeobj.length; i++)
	{
		if(gaugeobj[i].id === id)
			return gaugeobj[i];
	}
}
