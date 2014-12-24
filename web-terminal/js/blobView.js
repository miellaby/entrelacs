function BlobView(a, terminal, x, y) {
    var self = this;
    var server = terminal.entrelacs.serverUrl;
    var uri = a.serialize();
    var d = $("<div class='blob'></div>");
    var isContentTyped = (a.getTail() === Arrow.atom('Content-Typed'));
    var type = isContentTyped ? a.getHead().getTail().getBody() : null;
    var isCreole = type == "text/x-creole";
    var o;
    
    d.css({'left': x + 'px', 'top': y + 'px'});
    d.css({
            'margin-left': -parseInt(d.width() / 2) + 'px',
            'margin-top': -d.height() + 'px'
    });
    View.call(this, a, terminal, d);
    this.contentType = type;
    
    if (isCreole) {
        d.addClass('wiki');
        o = $("<div class='content' tabIndex=1><div class='scrollable'></div></div>");
        var contentArrow = a.getHead().getHead();
        if (contentArrow.url) { // placeholder
            // TODO turn this into entrelacs method (check content type)
            var promise = terminal.entrelacs.invoke("/escape+" + contentArrow.url);
            promise.done(function (atom) {
                contentArrow.replaceWith(atom);
                terminal.creole.parse(o.children('.scrollable')[0], atom.getBody());
                d.css({
                    'margin-left': -parseInt(d.width() / 2, 10) + 'px',
                    'margin-top': - d.height() + 'px'
                });
            });
        } else {
            terminal.creole.parse(o.children('.scrollable')[0], contentArrow.getBody());
        }
        o.appendTo(d);
       
    } else {
        var isImage = type && type.search(/^image/) == 0;
        var isOctetStream = type == "application/octet";
        if (isImage) {
            o = $("<img class='content' tabIndex=1></img>")
                 .attr('src', server + '/escape+' + uri);
            o.colorbox({
                href: terminal.entrelacs.serverUrl + '/escape+' + uri,
                photo: true});
                
            self.terminal.transfertCount++;
            self.terminal.moveLoadindBarOnView(self);
            self.terminal.loading.show();
            o.load(function() {
                if (self.terminal.transfertCount) self.terminal.transfertCount--;
                if (!self.terminal.transfertCount) self.terminal.loading.hide();
                
                if (isImage) o.appendTo(d);
                var wo = o.width();
                var ho = o.height();
                if (wo > ho)
                    o.css({width: '100px', 'max-height': 'auto'});
                else
                    o.css({height: '100px', 'max-width': 'auto'});
    
                var w = d.width();
                var h = d.height();
                d.css({
                    'margin-left': -parseInt(w / 2, 10) + 'px',
                    'margin-top': -h + 'px'
                });
            });
        } else {
            o = $("<a class='content' tabIndex=1></a>")
                .attr('href', server + '/escape+' + uri).text(type)
                .appendTo(d);
            o.colorbox({
                href: terminal.entrelacs.serverUrl + '/escape+' + uri,
                photo: isImage,
                iframe: true,
                innerWidth: "80%",
                innerHeight: "80%",
            });
        }
        
        
    }

    d.css({
        'margin-left': -parseInt(d.width() / 2, 10) + 'px',
        'margin-top': -d.height() + 'px'
    });
    
    var dblClickTimeout = null;
    $.extend(this.on, {
        click: function(event) {
            //event.stopPropagation();
            //return false;
            if (isCreole && dblClickTimeout === null) {
                 dblClickTimeout = setTimeout(function() {
                    dblClickTimeout = null;
                    $.colorbox({ inline: true, href: o });
                 }, 500);
            }
            return true;
        },
                
        dblclick: function(event) {
            if (dblClickTimeout !== null) {
                clearTimeout(dblClickTimeout);
                dblClickTimeout = null;
            }
            var prompt = self.edit();
            if (prompt) prompt.focus();
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
    
    o.click(this.on.click).dblclick(this.on.dblclick).focus(this.on.focus).blur(this.on.blur);
}

$.extend(BlobView.prototype, View.prototype, {
    isPairView: function() { return false; },

    edit: function() {
        var contentArrow = this.arrow.getHead().getHead();
        var promise;
        if (contentArrow.url) {
            // blob can't be edited
            // terminal.entrelacs.bindUri(contentArrow, contentArrowUri);
            promise = this.terminal.entrelacs.invoke("/escape+" + contentArrow.url);
        } else {
            promise = $.when(contentArrow);
        }

        var self = this;
        promise.done(function(atom) {
            var type = self.arrow.getHead().getTail().getBody();
            View.prototype.edit.call(self);
            var w = self.d.width(), h = self.d.height();
            var p = self.d.position();
            var prompt = new Prompt(atom.getBody(), self.terminal, p.left, p.top, true);
            prompt.d.children('textarea').css({'width': w + 'px', 'height': h + 'px'});
            prompt.d.css({'margin-left': -(w / 2) + 'px', 'margin-top': -h + 'px'});
            prompt.setWikiFormated(type == "text/x-creole");
            self.replaceWith(prompt);
            return prompt;
        });
        
        return promise;
    }


});
