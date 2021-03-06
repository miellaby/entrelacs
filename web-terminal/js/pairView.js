function PairView(a, terminal, tv, hv, root) {
    var p0 = tv.d.position();
    var p1 = hv.d.position();
    var x0 = p0.left + 6;
    var y0 = p0.top;
    var x1 = p1.left - 6;
    var y1 = p1.top;
    var d = $("<div class='" + (x0 < x1 ? "pair" : "ipair") + "'><div class='tail'><div class='tailEnd'></div></div><div class='head'><div class='headEnd'></div></div></div>"); // <div class='close'><a href='#'>&times;</a></div>
    var margin = Math.max(20, Math.min(50, Math.abs(y1 - y0)));
    d.css({
        'left': parseInt((x0 + x1) / 2, 10) + 'px',
        'margin-left': -parseInt(Math.abs(x1 - x0) / 2, 10) + 'px',
        'top': Math.max(y0, y1) + margin + 'px',
        'margin-top': -(Math.abs(y1 - y0) + margin) + 'px',
        'width': Math.abs(x1 - x0) + 'px',
        'height': (Math.abs(y1 - y0) + margin) + 'px',
    });

    View.call(this, a, terminal, d);
    this.tv = tv;
    this.hv = hv;
    
    if (terminal.animatePlease && !root) {
        d.hide();
        d.children('.tail').animate({'height': (margin + ((y0 < y1) ? y1 - y0 : 0)) + 'px'});
        d.children('.head').animate({'height': (margin + ((y1 < y0) ? y0 - y1 : 0)) + 'px'});
        d.fadeIn(500);
    } else {
        d.children('.tail').css({'height': (margin + ((y0 < y1) ? y1 - y0 : 0)) + 'px'});
        d.children('.head').css({'height': (margin + ((y1 < y0) ? y0 - y1 : 0)) + 'px'});
    }

    if (a) {
        tv.children.push(this);
        hv.children.push(this);
    } else {
        tv.for.push(this);
        hv.for.push(this);
    }
}

$.extend(PairView.prototype, View.prototype, {
    removeFor: function() {
        var ends = [this.tv, this.hv];
        for (var i = 0; i < 2; i++) {
            var end = ends[i];
            if (!end) continue;
            var list = end.for;
            for (var j = 0; j < list.length; j++) {
                var c = list[j];
                if  (c === this) {
                    list.splice(j, 1);
                    break;
                }
            }
        }
    },

    removeChild: function() {
        var ends = [this.tv, this.hv];
        for (var i = 0; i < 2; i++) {
            var end = ends[i];
            if (!end) continue;
            var list = end.children;
            for (var j = 0; j < list.length; j++) {
                var c = list[j];
                if  (c === this) {
                    list.splice(j, 1);
                    break;
                }
            }
        }
        return list;
    },

    adjust: function() {
        var tv = this.tv;
        var hv = this.hv;
        var p0 = tv.d.position();
        var p1 = hv.d.position();
        var x0 = p0.left + 6;
        var y0 = p0.top;
        var x1 = p1.left - 6;
        var y1 = p1.top;
        var d  = this.d;
        
        if (d.hasClass('ipair') && x0 < x1) {
            d.removeClass('ipair');
            d.addClass('pair');
        } else if (x0 >= x1) {
            d.removeClass('pair');
            d.addClass('ipair');
        }
        var margin = Math.max(20, Math.min(50, Math.abs(y1 - y0)));
        d.css({
            'left': parseInt((x0 + x1) / 2, 10) + 'px',
            'margin-left': -parseInt(Math.abs(x1 - x0) / 2, 10) + 'px',
            'top': Math.max(y0, y1) + margin + 'px',
            'margin-top': -(Math.abs(y1 - y0) + margin) + 'px',
            'width': Math.abs(x1 - x0) + 'px',
            'height': (Math.abs(y1 - y0) + margin) + 'px',
        });

        d.children('.tail').css({'height': (margin + ((y0 < y1) ? y1 - y0 : 0)) + 'px'});
        d.children('.head').css({'height': (margin + ((y1 < y0) ? y0 - y1 : 0)) + 'px'});

        var f = this.for;
        for (var i = 0; i < f.length; i++) {
            f[i].adjust();
        }
        var c = this.children;
        for (var i = 0; i < c.length; i++) {
            c[i].adjust();
        }
    },

    rewireEnd: function(oldOne, newOne) {
        var oldTail = this.tv;
        var oldHead = this.hv;

        if (oldTail === oldOne)
           this.tv = newOne;
        if (oldHead === oldOne)
            this.hv = newOne;
            
        this.adjust();
    },

    dismiss: function(direction) {
        if (this.detached) return;

        this.removeFor();
        this.removeChild();
        
        // propagate deletion to tail
        if (this.tv.isLoose())
            this.tv.dismiss(true);
        // propagate deletion to head
        if (this.hv.isLoose())
            this.hv.dismiss(true);
               
        View.prototype.dismiss.call(this, direction);
    },

    getGeometry: function() {
        var view = this.d;
        // TODO : should just get position of both end views
        var w = (view.hasClass('ipair') ? -1 : 1) * view.width();
        var ht = view.children('.tail').height();
        var hh = view.children('.head').height();
        var h = ht - hh;
        return Arrow.pair(
            Arrow.atom("" + w),
            Arrow.atom("" + h)
        );
    },
    
       
    setGeometry: function(floating, w, h) {
        var dt = this.tv.d;
        var dh = this.hv.d;
        var dtPos = dt.position();
        var dhPos = dh.position();
        // console.log('setPairViewGeometry ' + this.arrow.serialize() + ' ('
        //        + (floating == this.hv ? "head floating" : floating ? "tail floting" : "all floating") + ') ' + w + ' ' + h);
        if (floating) {
            if (floating == this.tv) {
                this.tv.move(
                        dhPos.left - w - dtPos.left,
                        dhPos.top - h - dtPos.top);
            } else {
                this.hv.move(
                        dtPos.left + w - dhPos.left,
                        dtPos.top + h - dhPos.top);
            }
        } else {
            var pos = this.d.position();
            pos = {
                left: pos.left,
                top: pos.top
            };
            var margin = Math.max(20, Math.min(50, h));
            this.tv.move(
                    pos.left - w / 2 - dtPos.left,
                    pos.top - margin - (h > 0 ? h: 0) - dtPos.top, this);
            this.hv.move(
                    pos.left + w / 2 - dhPos.left,
                    pos.top - margin + (h < 0 ? h: 0) - dhPos.top);
        }
    },

    confirm: function() {
        if (this.arrow) return this;

        var fromView = this.tv.confirm();
        var toView = this.hv.confirm();
        this.arrow = Arrow.pair(fromView.arrow, toView.arrow);
        this.bind();
        fromView.removeFor(this);
        toView.removeFor(this);
        fromView.children.push(this);
        toView.children.push(this);
        return this;
    },

    isPairView: function() { return true; }
});
