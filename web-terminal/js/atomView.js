function AtomView(a, terminal, x, y) {
    var self = this;
    var v = a.getBody();
    var preformated = (v === ' ' || ~v.indexOf("\n") || ~v.indexOf("\t") || ~v.indexOf("  "));
    var d = preformated
    ? $("<div class='atom'><pre class='content' tabIndex=1></pre><div class='close'><a href='#'>&times;</a></div></div>")
     : $("<div class='atom'><span class='content' tabIndex=1></span><div class='close'><a href='#'>&times;</a></div></div>");
    d.css({
        'left': x + 'px',
        'top': y + 'px',
    });
    View.call(this, a, terminal, d);

    var s = d.children('.content');
    s.text(a.getBody() || 'EVE');
    var w = Math.max(s.width(), 100);
    s.css({width: '' + w + 'px'});
    d.css({
        'margin-left': -parseInt(d.width() / 2, 10) + 'px',
        'margin-top': -d.height() + 'px'
    });
    d.attr('draggable', true);
    $.extend(this.on, {
        click: function(event) {
            event.stopPropagation();
        },
                
        dblclick: function(event) {
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
    
    s.click(this.on.click).dblclick(this.on.dblclick).focus(this.on.focus).blur(this.on.blur);
}

$.extend(AtomView.prototype, View.prototype, {
    focus: function() {
        this.d.children('.content').focus();
        return true;
    },
    
    edit: function() {
        var v = (this.arrow
                 ? this.arrow.getBody()
                 : this.d.children('.content').text());
        View.prototype.edit.call(this);
    
        var p = this.d.position();
        var prompt = new Prompt(v, this.terminal, p.left, p.top, true);
        this.replaceWith(prompt);
        return prompt;
    },

    isPairView: function() { return false; }
});
