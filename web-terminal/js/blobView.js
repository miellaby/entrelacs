function BlobView(a, terminal, x, y) {
    var self = this;
    var server = terminal.entrelacs.serverUrl;
    var uri = Arrow.serialize(a);
    var d = $("<div class='blob'></div>");
    var isContentTyped = (a.getTail() === Arrow.atom('Content-Typed'));
    var type = isContentTyped ? a.getHead().getTail().getBody() : null;
    var isImage = type && type.search(/^image/) == 0;
    var isOctetStream = type == "application/octet";
    var isCreole = type == "text/x-creole";
    var o = (isImage
        ? $("<img class='content' tabIndex=1></img>").attr('src', server + '/escape+' + uri)
            : (isCreole
            ? $("<div class='content' tabIndex=1></div>")
            : (isOctetStream
                ? $("<a class='content' target='_blank' tabIndex=1></a>").attr('href', server + '/escape+' + uri).text(uri)
                : $("<object class='content' tabIndex=1></object>").attr('data', server + '/escape+' + uri))));

    d.css({'left': x + 'px', 'top': y + 'px'});
    d.css({
            'margin-left': -parseInt(d.width() / 2) + 'px',
            'margin-top': -d.height() + 'px'
    });
    View.call(this, a, terminal, d);
    this.contentType = type;
    
    if (isCreole) {
        var contentArrow = a.getHead().getHead();
        if (!contentArrow.getBody) { // placeholder
            var promise = terminal.entrelacs.invoke("/escape+" + contentArrow.uri);
            promise.done(function (text) {
                terminal.creole.parse(o[0], text);
                self.unbind();
                self.arrow = Arrow.atom(text);
                self.bind();
                var w = d.width();
                var h = d.height();
                d.css({
                    'margin-left': -parseInt(w / 2) + 'px',
                    'margin-top': -h + 'px'
                });
                o.appendTo(d);
            });
        } else {
            terminal.creole.parse(o[0], contentArrow.getBody());
            o.appendTo(d);
            var w = d.width();
            var h = d.height();
            d.css({
                'margin-left': -parseInt(w / 2) + 'px',
                'margin-top': -h + 'px'
            });
        }
    } else {
        o.colorbox({href: terminal.entrelacs.serverUrl + '/escape+' + uri, photo: isImage });
        self.terminal.transfertCount++;
        self.terminal.moveLoadindBarOnView(self);
        self.terminal.loading.show();
        o.load(function() {
            if (self.terminal.transfertCount) self.terminal.transfertCount--;
            if (!self.terminal.transfertCount) self.terminal.loading.hide();
            
            o.appendTo(d);
            var wo = o.width();
            var ho = o.height();
            if (wo > ho)
                o.css({width: '100px', 'max-height': 'auto'});
            else
                o.css({height: '100px', 'max-width': 'auto'});

            var w = d.width();
            var h = d.height();
            d.css({
                'margin-left': -parseInt(w / 2) + 'px',
                'margin-top': -h + 'px'
            });
        });
    }

    
    $.extend(this.on, {
        click: function(event) {
            event.stopPropagation();
        },
                
        dblclick: function(event) {
            self.edit();
            return false;
        },
                
        focus: function(event) {
            self.bar.children('div,a').show().animate({'height': self.bar.height(), opacity: 1}, 200);
        },
        
        blur: function(event) {
            if (!self.bar.hasClass('hover'))
                self.bar.children('div,a').animate({'height': '0px', opacity: 0}, 500);
        }
    });
    if (isCreole) {
        o.click(this.on.click).dblclick(this.on.dblclick).focus(this.on.focus).blur(this.on.blur);
    };
}

$.extend(BlobView.prototype, View.prototype, {
    isPairView: function() { return false; },

    edit: function() {
        if (!this.arrow.getHead().getHead().getBody) {
            // blob can't be edited
            return this;
        }
        
        var v = this.arrow.getHead().getHead().getBody();
        View.prototype.edit.call(this);
      
        var p = this.d.position();
        var prompt = new Prompt(v, this.terminal, p.left, p.top);
        prompt.setWikiFormated(true);
        this.replaceWith(prompt);
        return prompt;
    }


});
