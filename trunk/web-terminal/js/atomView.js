function AtomView(a, terminal, x, y) {
    var self = this;
    
    var d = $("<div class='atom'><span class='content' tabIndex=1></span><div class='close'><a href='#'>&times;</a></div></div>");
    d.css({
        'left': x + 'px',
        'top': y + 'px',
    });
    View.call(this, a, terminal, d);

    var s = d.children('span');
    s.text(a.getBody() || 'EVE');
   var w = Math.max(s.width(), 100);
    s.css({width: '' + w + 'px', 'text-align': 'center'});
    d.css({
        'margin-left': -parseInt(d.width() / 2) + 'px',
        'margin-top': -d.height() + 'px'
    });
    d.attr('draggable', true);
    $.extend(this.on, {
        click: function(event) {
            event.stopPropagation();
        },
        
        focus: function(event) {
            self.bar.children('div,a').show().animate({'height': self.bar.height(), opacity: 1}, 200);
        },
        
        blur: function(event) {
            if (!self.bar.hasClass('hover'))
                self.bar.children('div,a').animate({'height': '0px', opacity: 0}, 500);
        }
    });
    
    d.click(this.on.click);
    s.focus(this.on.focus).blur(this.on.blur);
}

$.extend(AtomView.prototype, View.prototype, {
    isPairView: function() { return false; }
});
