function Terminal(area, entrelacs, animatePlease) {
    var self = this;
    this.animatePlease = animatePlease || false;
    this.area = area;
    this.entrelacs = entrelacs;
    this.area.data('terminal', this);

    var protoEntry = $("<div class='prompt'><input type='text' /><span class='fileinput-button'><span>...</span><input type='file' name='files[]'> /></span></div>");
    protoEntry.appendTo(area);
    this.defaultEntryWidth = protoEntry.width();
    this.defaultEntryHeight = protoEntry.height();
    protoEntry.detach();
    this.loading = $("<div class='loading bar' id='loading'><div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div></div>");
    this.loading.appendTo(area);
    // TODO: move out connection button
    if (window.location.hash) {
        entrelacs.invoke("/escalate/escape//mudo+chut//fall+/escape+demo/,/land+").done(function() {
            self.show(Arrow.atom(window.location.hash.substr(1)),  area.width() / 2, area.height() / 2).update();
        });
    } else {
        this.connect = $("<p class='connect' align='center'><button id='go'>...</button></p>");
        this.connect.children('button').click(function() {
            alert("Leaving sand box. Entering public area ...");
            window.location = "#pub";
            var promise = entrelacs.invoke("/escalate/escape//mudo+chut//fall+/escape+demo/,/land+");
            promise.done(function() { window.location.reload(); });
            return false;
        });
        this.connect.appendTo(area);
    }
    this.circleAngle = 0;
    this.uploadCount = 0;

    this.creole = new Parse.Simple.Creole({forIE: document.all,
                                     interwiki: {
                WikiCreole: 'http://www.wikicreole.org/wiki/',
                Wikipedia: 'http://en.wikipedia.org/wiki/'},
                                     linkFormat: '' });

    Arrow.listeners.push(function(a) { self.arrowEvent(a); });
    $(document).ajaxStart(function(){
        self.loading.show();
    }).ajaxStop(function(){
        if (!self.uploadCount) self.loading.hide();
    });
    
    this.commitTimeout = null;


    var mx0, my0, deltaSum;
    
    this.on = {
        area: {
            click: function(event) {
                if (deltaSum < 5) {
                    var x = event.pageX;
                    var y = event.pageY;
                    var p = new Prompt("", self, x, y);
                    p.focus();
                }
            },
            mousedown: function(event) {
                if (event.target != area[0]) return;
                
                mx0 = event.pageX;
                my0 = event.pageY;
                deltaSum = 0;
                area.on("mousemove", self.on.area.mousemove);
                return true;
            },
            mouseup: function(event) {
                area.off('mousemove', self.on.area.mousemove);
                return true;
            },
            mouseleave: function(event) {
                area.off('mousemove', self.on.area.mousemove);
                deltaSum = 0;
                return true;
            },
            mousemove: function(event) {
              var dx = event.pageX - mx0;
              var dy = event.pageY - my0;
              mx0 = event.pageX;
              my0 = event.pageY;
              deltaSum += Math.abs(dx) + Math.abs(dy);
              if (deltaSum > 5) {
                self.scroll(dx, dy);
              }
                
              return true;
            }
        }
    }
    area.click(this.on.area.click);
    area.mousedown(this.on.area.mousedown);
    area.mouseup(this.on.area.mouseup);
    area.mouseleave(this.on.area.mouseleave);
}

Terminal.prototype = {

    show: function(a, x, y, ctx) {
        var view = this.findNearestArrowView(a, {left: x, top: y}, 1000);
        if (view) return view;
        
        if (a.uri !== undefined) { // placeholder
            view = new PlaceholderView(a, this, x, y);
        } else if (a.isAtomic()) {
            view = new AtomView(a, this, x, y);
        } else if (a.getTail() == Arrow.atom('Content-Typed')) {
            view = new BlobView(a, this, x, y);
        } else {
            var tv = this.show(a.getTail(), x - 100 - this.defaultEntryWidth, y - 170, ctx);
            var hv = this.show(a.getHead(), x + 100, y - 130, ctx);
            view = new PairView(a, this, tv, hv, ctx);
            view.restoreGeometry(null, ctx || view);
        }

        return view;
    },

    commit: function() {
        if (this.commitTimeout !== null) {
            clearTimeout(this.commitTimeout);
            this.commitTimeout = null;
        }
        Arrow.commit(this.entrelacs);
    },
    
    prepareCommit: function(d) {
        var self = this;
        if (this.commitTimeout !== null) {
            clearTimeout(this.commitTimeout);
        }
        
        this.commitTimeout = setTimeout(function() {
            self.commitTimeout = null;
            self.commit();
        }, 5000);
    },
    
    findNearestArrowView: function(arrow, position, limit) {
        var views = arrow.get('views');
        if (views === undefined || !views.length) return null;
        var distance = -1;
        var nearest = null;
        views.forEach(function(view) {
            if (view.d.hasClass('placeholder')) return;
            
            var vp = view.d.position();
            var vd = Math.abs(vp.left - position.left) + Math.abs(vp.top - position.top);
            if (distance == -1 || vd < distance) {
                distance = vd;
                nearest = view;
            }
        });
        
        return nearest
            ? (limit !== undefined && distance > limit ? null : nearest)
            : null;
    },
    
    getPointOnCircle: function(radius) {
        this.circleAngle += 4;
        var x = radius * Math.sin(2 * Math.PI * this.circleAngle / 23);
        var y = radius * Math.cos(2 * Math.PI * this.circleAngle / 23);
        var p = { x: parseInt(x), y: parseInt(y)};
        return p;
    },

    leave: function(view, incoming) {
       var d = view.d;
       var p = d.position();
       var c = this.getPointOnCircle(100);
       var n, a;
       if (incoming) {
          n = new Prompt("", this,
                    p.left - 100 - c.x,
                    p.top - c.y, true /* immediate */);
          a = new PairView(null, terminal, n, view);
       } else {
          n = new Prompt("", this,
                    p.left + 100 + c.x,
                    p.top + c.y, true /* immediate */);
          a = new PairView(null, terminal, view, n);
       }
       n.focus();
       return a;
    },

    // find an editable atom into a tree
    findNextInto: function(view) {
        if (!view.isPairView() && view.arrow === null) { // prompt
            return view;
        } else if (view.isPairView()) { // pair
           // check in tail then in head
           var n = this.findNextInto(view.tv);
           if (!n)
                n = this.findNextInto(view.hv);
           return n;
        }
        return null;
    },

    // search for the next editable atom
    findNext: function(view, from, leftOnly) {
       var f = view.for;
       if (view.isPairView() && view.hv !== from) { // search into head
          var n = this.findNextInto(view.hv);
          if (n) return n;
       }
       
       for (var i = 0; i < f.length; i++) { // search into loose children
            var n = this.findNext(f[i], view, leftOnly);
            if (n) return n;
       }
       
      if (!leftOnly && view.isPairView() && view.tv !== from) { // search into tail
          var n = this.findNextInto(view.tv);
          if (n) return n;
       }

       // search fail: no editable
       return null;
    },
    
    // find previous editable atom into a tree
    findPreviousInto: function(view) {
        if (!view.isPairView() && view.arrow === null) { // prompt
            return view;
        } else if (view.isPairView()) { // pair
           // check in head then in tail
           var n = this.findPreviousInto(view.hv);
           if (!n)
                n = this.findPreviousInto(view.tv);
           return n;
        }
        return null;
    },
    

    // search for the previous editable atom
    findPrevious: function(view, from, rightOnly) {
       var f = view.for;
       if (view.isPairView() && view.tv !== from) { // search into tail
          var n = this.findPreviousInto(view.tv);
          if (n) return n;
       }
       
       for (var i = 0; i < f.length; i++) { // search into loose children
            var n = this.findPrevious(f[i], view, rightOnly);
            if (n) return n;
       }
       
      if (!rightOnly && view.isPairView() && view.hv && view.hv !== from) { // search into tail
          var n = this.findPreviousInto(view.hv);
          if (n) return n;
       }

       // search fail: no editable
       return null;
    },


    moveLoadindBarOnView: function(view) {
       var p = view.d.position();
       this.loading
            .css('top', (p.top) + 'px')
            .css('left', (p.left + view.d.width() / 2 - (this.loading.width() * 0.5)) + 'px');
    },

    arrowEvent: function(a) {
        if (a === null) { // reset!
            // TODO: move out reconnection
            if (window.location.hash) {
                var promise = this.entrelacs.invoke("/escalate/escape//mudo+chut//fall+/escape+demo/,/land+");
            }
            return;
        }
        var views = a.get('views');
        if (a.hc === undefined ) { // a is GC-ed
            views && views.forEach(function(view) {
                view.edit();
            });
        } else {
            var r = a.isRooted();
            views && views.forEach(function(view) { view.d.find('.toolbar .rooted input').prop('checked', r); });
        }
    },    

    scroll: function(dx, dy) {
        var i = 0;
        this.area.children().each(function(i, element) {
            element = $(element)
            var position = element.position();
            element.css({left: (position.left + dx) + 'px', top: (position.top + dy) + 'px'});
        });
    }
};
