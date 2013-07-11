function BlobView(a, terminal, x, y) {
    var server = terminal.entrelacs.serverUrl;
    var uri = Arrow.serialize(a);
    var d = $("<div class='blob'></div>");
    var isContentTyped = (a.getTail().getTail() === Arrow.atom('Content-Typed'));
    var type = isContentTyped ? a.getHead().getTail().getBody() : null;
    var isImage = isContentTyped && type.search(/^image/) == 0;
    var isOctetStream = isContentTyped && type == "application/octet";
    var isCreole = isContentTyped && type == "text/x-creole";
    var o = (isImage
        ? $("<img class='content' tabIndex=1></img>").attr('src', server + '/escape+' + uri)
            : (isCreole
            ? $("<div class='content' tabIndex=1></div>")
            : (isOctetStream
                ? $("<a class='content' target='_blank' tabIndex=1></a>").attr('href', server + '/escape+' + uri).text(uri)
                : $("<object class='content' tabIndex=1></object>").attr('data', server + '/escape+' + uri)))).appendTo(d);
    if (isCreole) {
        terminal.creole.parse(o[0], a.getHead().getHead().getBody());
    } else {
        o.css('height', 100);
    }
    this.setTerminal(terminal);

    if (isCreole) {
        o.click(this.on.click);
    } else {
        o.colorbox({href: this.entrelacs.serverUrl + '/escape+' + uri, photo: isImage });
        var wo = o.width();
        var ho = o.height();
        if (wo > ho)
            o.css({width: '100px', 'max-height': 'auto'});
        else
            o.css({height: '100px', 'max-width': 'auto'});
    }

    var w = d.width();
    var h = d.height();
    d.css('left', (x - Math.round(w / 2, 0)) + 'px');
    d.css('top', (y - h) + 'px');

    View.call(this, a, terminal, d);
}

$.extend(BlobView.prototype, View.prototype, {
    isPairView: function() { return false; }

});
