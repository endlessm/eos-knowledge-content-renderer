$(function() {
	// Remove all empty paragraphs
	$('.mw-parser-output p').filter(function () { return $.trim(this.innerHTML) == "" }).remove();

	// Add a wrapper div around first table infobox
	$(".infobox:first").wrap("<div class='infobox-toggle-wrapper'><div class='infobox-toggle'></div></div>");

	// Add a checkbox to handle infotable visibility state
	$("<input id='toggle' type='checkbox' checked><label for='toggle' class='toggle_label'>Quick facts</label>").insertBefore(".infobox-toggle:first");
});
