function PlaceholderView(a, terminal, x, y) {
    var self = this;
    var d = $("<div class='placeholder'><button class='unfold'>+</button></div>");
    d.css({left: x + 'px', top: y + 'px'});
    View.call(this, a, terminal, d);
    d.css({
        'margin-left': -parseInt(d.width() / 2) + 'px',
        'margin-top': -(d.height() + 5) + 'px'
    });
    d.children('button').attr('draggable', true);
              
    $.extend(this.on, {
        unfold: {
            click: function(event) {
                self.unfold();
                return false;
            }
        }
    });
    d.children('.unfold').click(this.on.unfold.click);
}
$.extend(PlaceholderView.prototype, View.prototype, {
    isPairView: function() { return false; },

    unfold: function() {
        var self = this;
        var arrow = this.arrow;
        var terminal = this.terminal;
        var p = this.d.position();
        if (arrow.uri !== undefined) { // an unknown placeholder
            var promise = terminal.entrelacs.open(arrow);
            promise.done(function(unfolded) {
                var view = terminal.show(unfolded, p.left, p.top);
                self.replaceWith(view);
            });
        } else { // a folded known arrow
            var view = terminal.show(arrow, p.left, p.top);
            self.replaceWith(view);
        }
    }
});
