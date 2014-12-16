function View(arrow, terminal, d) {
    this.arrow = arrow;
    this.terminal = terminal;
    this.d = d;
    this.for = [];
    this.children = [];
    this.tv = null;
    this.hv = null;
    
    d.data('view', this);

    var self = this;
    this.on = {
        bar: {
            click: function(event) {
                var button = $(this);
                if (button.hasClass('toolbar')) {
                   event.stopPropagation();
                   return true;
                }
                if (button.hasClass('rooted')) {
                    return true;
                }
                if (button.hasClass('close')) {
                    self.close();
                }  else if (button.hasClass('poke')) {
                    self.update();
                } else if (button.hasClass('in')) {
                    self.terminal.leave(self, true);
                } else if (button.hasClass('out')) {
                    self.terminal.leave(self, false);
                }
                return false;
            },
            rooted: {
                change: function(event) {
                    var arrow = self.arrow;
                    if (!arrow) {
                        arrow = self.confirm().arrow;
                    }
                    if (arrow) {
                        if (arrow.isRooted()) {
                            arrow.unroot();
                            self.cleanAllGeometryAfterUnroot(arrow);
                            if (!self.children.length) {
                                self.close();
                            }
                        } else {
                            arrow.root();
                            self.saveAllGeometryAfterRoot(arrow);
                        }
                        self.terminal.commit();
                        return true;
                    } else
                        return false;
                },
            },
            enter: function(event) {
                var d = self.d;
                var bar = self.bar;
                bar.addClass('hover').children('div,a').show().css({'height': bar.height()}).animate({opacity: 1}, 50);
                if (d.is('.pair,.ipair')) {
                    d.children('.tail').css('border-color', '#444').delay(50).css('border-color', '#aaa').delay(50).css('border-color', '#ddd')
                        .children('.tailEnd').css('border-top-color', '#444').delay(50).css('border-top-color', '#aaa').delay(50).css('border-top-color', '#ddd');
                    d.children('.head').css('border-color', '#444').delay(50).css('border-color', '#aaa').delay(50).css('border-color', '#ddd')
                        .children('.headEnd').css('border-bottom-color', '#444').delay(50).css('border-bottom-color', '#aaa').delay(50).css('border-bottom-color', '#ddd');
                }
                return true;
            },
            leave: function(event) {
                var d = self.d;
                var bar = self.bar;
                bar.removeClass('hover');
                if (!self.d.children('input,textarea').is(':focus'))
                    bar.children('div,a').css({'height': bar.height()}).animate({opacity: 0}, 200);
                if (d.is('.pair,.ipair')) {
                    d.children('.tail').css('border-color', '#aaa').delay(50).css('border-color', '#444').delay(50).css('border-color', '#000')
                        .children('.tailEnd').css('border-top-color', '#aaa').delay(50).css('border-top-color', '#444').delay(50).css('border-top-color', '#000');
                    d.children('.head').css('border-color', '#aaa').delay(50).css('border-color', '#444').delay(50).css('border-color', '#000')
                        .children('.headEnd').css('border-bottom-color', '#aaa').delay(50).css('border-bottom-color', '#444').delay(50).css('border-bottom-color', '#000');
                }
                return true;
            }
        },
        
        dragstart: function(event) {
            if (event.currentTarget != self.d[0]) {
                console.log('not');
                event.preventDefault();
                    return;
            }
            
            console.log('dragstart');
            // dragStartX = ($.browser.mozilla ? e.originalEvent.screenX : e.originalEvent.pageX);
            // dragStartY = ($.browser.mozilla ? e.originalEvent.screenY : e.originalEvent.pageY);

            self.terminal.dragStartX = event.originalEvent.screenX;
            self.terminal.dragStartY = event.originalEvent.screenY;
            var data = (self.arrow ? Arrow.serialize(self.arrow) : self.d.children("input[type='text'],textarea").val());
            var bugInducingElement = self.d.children("input[type='text'],textarea");
            bugInducingElement.css('visibility', 'hidden');
            event.originalEvent.dataTransfer.setData('text/plain', data);
            event.originalEvent.dataTransfer.effectAllowed = "move";
            event.stopPropagation();
        },
        
        dragend: function(event) {
            console.log('dragend');
            var bugInducingElement = self.d.children("input[type='text'],textarea");
            bugInducingElement.css('visibility', 'visible');

            if (self.terminal.dragOver) {
                if (self.terminal.dragOver != self) {
                    self.terminal.dragOver.replaceWith(self);
                }
                self.terminal.dragOver = null;
            } else {
                // dragEndX = ($.browser.mozilla ? e.originalEvent.screenX : e.originalEvent.pageX) - dragStartX
                // dragEndY = ($.browser.mozilla ? e.originalEvent.screenY : e.originalEvent.pageY) - dragStartY
                var dragEndX = event.originalEvent.screenX, dragEndY = event.originalEvent.screenY;

                self.move(dragEndX - self.terminal.dragStartX,
                          dragEndY - self.terminal.dragStartY, null /* no moving child */);
                self.saveChildrenGeometry();
                self.terminal.prepareCommit();
            }
            event.stopPropagation();
        },
        
        close: {
            click: function(event) {
                self.close();
                return false;
            }
        }
    };

    this.addBar();

    d.attr('draggable', !this.isPairView());
    d.on('dragstart', this.on.dragstart);
    d.on('dragend', this.on.dragend);
    d.find('.close a').click(this.on.close.click);

    d.appendTo(terminal.area);
    this.bind();
}

View.prototype = {
    addBar: function() {
        var d = this.d;
        var h = $("<div class='toolbar'><a class='in'>&uarr;</a> <div class='rooted'><input type='checkbox'></div> <a class='poke'>∊</a> <a class='close'>&times;</a> <a class='out'>&darr;</a></div>");
        h.attr('draggable', this.isPairView());
        h.appendTo(d);
        h.hover(this.on.bar.enter, this.on.bar.leave);
        h.children('a').click(this.on.bar.click);
        h.find('.rooted input').change(this.on.bar.rooted.change);
        h.click(this.on.bar.click);
        this.bar = h;
    },

    bind: function() {
        var arrow = this.arrow;
        if (!arrow) return;
        var views = arrow.get('views');
        if (views === undefined) {
            views = [];
            arrow.set('views', views);
        }
        views.push(this);
        
        console.log(Arrow.serialize(arrow) + " " + views.length + " views");
        
        // re-binding
        if (this.replaced && this.replaced != this.arrow) {
            var replacedWasRooted = false;
            if (this.replaced.hc !== undefined) {
                replacedWasRooted = this.replaced.isRooted();
                this.replaced.unroot();
                this.cleanAllGeometryAfterUnroot(this.replaced);
            }
            delete this.replaced;
            if (replacedWasRooted) {
                this.arrow.root();
                this.saveAllGeometryAfterRoot(this.arrow);
                this.terminal.commit();
            }
        }
        
        this.d.find('.toolbar .rooted input').prop('checked', arrow.isRooted());
    },
    
    unbind: function() {
        var arrow = this.arrow;
        if (!arrow) return;
        var vs = arrow.get('views');
        if (!vs) return;
        vs.splice(vs.indexOf(this), 1);
        console.log(Arrow.serialize(arrow) + " " + vs.length + " views");
        this.arrow = null;
    },
    
    rebind: function(a) {
        this.unbind();
        this.arrow = a || null;
        this.bind();
    },
    
    detach: function() {
        // detach this.d
        this.d.remove();
    },
    
    isLoose: function() {
        // Check view is in no tree and corresponding arrow is not rooted
        return (!this.for.length && !this.children.length && !(this.arrow && this.arrow.isRooted()));
    },
    
    dismiss: function(direction /* false = down true = up */) {
        // guard double detach
        if (this.detached) return;
        this.detached = true;
        
        // fold if children
        if (this.children.length) {
            var d = this.d;
            // TODO pourquoi d.position() ne fonctionne pas?
            var x = parseInt(d.css('left'), 10);
            var y = parseInt(d.css('top'), 10);
            var placeholder = new PlaceholderView(this.arrow, this.terminal, x, y);
            this.replaceWith(placeholder);
        } else { // no child
            if (this.for.length > 1) {
               // several prompt trees
                // propagate to loose "prompt trees"
                if (!direction) { //  when going "down"
                    while (this.for.length) {
                        this.for[0].dismiss();
                    }
                }
            } else if (this.for.length == 1) {
                // one pair end
                var pair = this.for[0];
                if (pair.replaced) {
                    pair.dismiss();
                } else {
                    pair.removeFor();
                    var otherEnd = (pair.tv == this ? pair.hv : pair.tv);
                    pair.replaceWith(otherEnd);
                    otherEnd.focus();
                }
            }
            
            this.unbind();
            this.detach();
        }
    },

    move: function(offsetX, offsetY, movingChild) {
        var d = this.d;
        var p = d.position();
        d.css({ 'opacity': 1,
                'left': (0 + p.left  + offsetX) + 'px',
                'top': (0 + p.top + offsetY) +'px'
        });
        if (this.hv) {
            this.hv.move(offsetX, offsetY, this);
            if (this.tv !== this.hv) {
                this.tv.move(offsetX, offsetY, this);
            }
        }
        var f = this.for;
        for (var i = 0; i < f.length; i++) {
            if (f[i] == movingChild) continue;
            f[i].adjust();
        }
        var c = this.children;
        for (var i = 0; i < c.length; i++) {
            if (c[i] == movingChild) continue;
            c[i].adjust();
        }
    },

    updateChild: function(a, newA) {
        var i = this.children.indexOf(a);
        if (~i) {
            this.children.splice(i, 1, newA);
        }
    },
    
    removeFor: function(a) {
        var i = this.for.indexOf(a);
        if (~i) {
           this.for.splice(i, 1);
        }
    },

    updateFor: function(a, newA) {
        var i = this.for.indexOf(a);
        if (~i) {
           this.for.splice(i, 1, newA);
        }
    },

    updateRefs: function(oldOne, newOne) {
        this.updateFor(oldOne, newOne);
        this.updateChild(oldOne, newOne);
    },

    /**
    * save all arrow geometries (width and height) which got modified after an ancestor move
    * Considering a moving arrow:
    * - Directly rooted children got their geometry data directly attached to themselves.
    * - Every children $a gets its geometry attached to every rooted descendant '$root' as 
    * ///geometry+$root/geometry+$a/$width+$height
    */
    saveChildrenGeometry: function() {
        var a = this.arrow;
        if (!a) return;

        /** recursive function called by saveChildrenGeomtry
        * so to attach children geometry to rooted descendants
        */
        var attachGeometryToDescendants = function(child, descendant) {
           var da = descendant.arrow;
           if (!da) return;
           if (da.isRooted()) {
                var key;
                if (da == child.arrow) {
                    key = Arrow.pair(Arrow.atom("geometry"), da);
                } else {
                    key = Arrow.pair(
                        Arrow.pair(Arrow.atom("geometry"), da),
                        Arrow.pair(Arrow.atom("geometry"), child.arrow));
                }
                Arrow.eveIfNull(key.getChild(Arrow.FILTER_INCOMING | Arrow.FILTER_UNROOTED)).unroot();
                Arrow.pair(key, child.getGeometry()).root();
            }
            var list = descendant.children;
            for (var i = 0; i < list.length; i++) {
               var c = list[i];
               attachGeometryToDescendants(child, c);
            }
        };
        var list = this.children;
        for (var i = 0; i < list.length; i++) {
           var c = list[i];
           attachGeometryToDescendants(c, c);
        }
    },

    /** when unrooting the arrow, one removes all saved geometry
    */
    cleanAllGeometryAfterUnroot: function(r) {
        if (!r) return;
        var p = Arrow.pair(Arrow.atom("geometry"), r);
        var it = p.getChildren(Arrow.FILTER_INCOMING);
        var c, c2;
        var isP = this.isPairView();
        while ((c = it()) != null) {
            if (c.isRooted()) { // case: geometry+root/width+height
                c.unroot();
            }
            if (isP) {
                // case: ///geometry+root/geometry+someArrow/width+height
                var it2 = c.getChildren(Arrow.FILTER_INCOMING | Arrow.FILTER_UNROOTED);
                while ((c2 = it2()) != null) {
                    c2.unroot();
                }
            }
        }
    },
    
    /** when rooting an arrow, one saves all ancesters geometry
    */
    saveAllGeometryAfterRoot: function(r) {
        if (!r) return;
        var self = this;
        var recSave = function(view) {
            if (view === undefined) return;
            var a = view.arrow;
            if (!a) return;
            if (view.getGeometry) {
                var key = Arrow.pair(
                    Arrow.pair(Arrow.atom("geometry"), r),
                    Arrow.pair(Arrow.atom("geometry"), a)
                );
                Arrow.eveIfNull(key.getChild(Arrow.FILTER_INCOMING | Arrow.FILTER_UNROOTED)).unroot();
                Arrow.pair(key, view.getGeometry()).root();
            }
            if (view.isPairView()) {
                recSave(view.tv);
                recSave(view.hv);
            }
        };
        if (this.isPairView()) {
            recSave(this.tv);
            recSave(this.hv);
        }
        if (this.getGeometry) {
            var key = Arrow.pair(Arrow.atom("geometry"), r);
            Arrow.eveIfNull(key.getChild(Arrow.FILTER_INCOMING | Arrow.FILTER_UNROOTED)).unroot();
            Arrow.pair(key, this.getGeometry()).root();
        }
    },
    
    /**
    * restore the geometry (width and height)
    * if the arrow is rooted, one searchs for the geometry attached to the /root+a combination
    * (actually //geometry+a/width+height for better indexing)
    */
    restoreGeometry: function(floating, root) {
        if (!this.setGeometry) return;
        
        var a = this.arrow;
        if (!a || !a.isRooted()) return;

        var key;
        if (root !== this) {
            key = Arrow.pair(Arrow.pair(Arrow.atom("geometry"), root),
                           Arrow.pair(Arrow.atom("geometry"), a));

        } else {
            key = Arrow.pair(Arrow.atom("geometry"), a);
        }
        
        console.log("check " + Arrow.serialize(key));
        var it = key.getChildren(Arrow.FILTER_INCOMING | Arrow.FILTER_UNROOTED);
        var c;
        if ((c = it()) != null) {
            console.log(Arrow.serialize(c));
            c = c.getHead();
            var w = parseInt(c.getTail().getBody(), 10);
            var h = parseInt(c.getHead().getBody(), 10);
            this.setGeometry(floating, w, h);
        }
    },
 
    replaceInDescendants: function(newA, except) {
        var f = this.for;
        // rewire 'for' descendants
        for (var i = 0; i < f.length; i++) {
            if (f[i] !== except)
                f[i].rewireEnd(this, newA);
        }
        
        // rewire children
        var children = this.children;
        for (var i = 0; i < children.length; i++) {
            if (children[i] !== except)
                children[i].rewireEnd(this, newA);
        }
    },

    replaceWith: function(newA) {
       this.unbind();

        var dedupInArray = function(arr) {
            var dedup = [];
            var l = arr.length;
            var marker = {};
            for (var i = 0; i < l; i++) {
                var o = arr[i];
                if (o.dOIAMark !== marker) {
                    dedup.push(arr[i]);
                    o.dOIAMark = marker;
                }
            }
            l = dedup.length;
            for (var i = 0; i < l; i++) {
                delete dedup[i].dOIAMark;
            }
            return dedup;
        };

        // descendants rewiring to newA
        newA.for = dedupInArray(newA.for.concat(this.for));
        newA.children = newA.children.concat(this.children);
        if (this.replaced) {
            if (newA.arrow) {
                console.log('replacing');
                if (this.replaced.hc !== undefined) {
                    console.log('unroot replaced');
                    this.replaced.unroot();
                    this.cleanAllGeometryAfterUnroot(this.replaced);
                }
                delete this.replaced;
                newA.arrow.root();
            } else {
                console.log('pass replaced');
                newA.replaced = this.replaced;
            }
           this.terminal.commit();
        }
        this.replaceInDescendants(newA);
        this.detach();
    },

        
    confirm: function() {
        return this;
    },

    confirmDescendants: function() {
        var self = this;
        var f = this.for;
        if (!f.length) { // terminal case: tree to be recorded
            var confirmedView = this.confirm();
            var promise = confirmedView.update();
            promise.done(function() {
                if (confirmedView.arrow === null) {
                    console.log('!? GCed');
                } else {
                    self.terminal.entrelacs.getUri(confirmedView.arrow);
                    console.log('root it');
                    confirmedView.arrow.root();
                    confirmedView.saveAllGeometryAfterRoot(confirmedView.arrow);
                }
            }).fail(function() {
                    confirmedView.arrow.root();
            });
            return [promise];
        } else {
            // get back to the roots (recursive calls to all waiting prompt arrows)
            var promises=[];
            for (var i = 0 ; i < f.length; i++) {
                promises = promises.concat(f[i].confirmDescendants());
            }
            return promises;
        }
    },

    edit: function() {
        if (this.arrow && this.arrow.isRooted()) {
            this.replaced = this.arrow;
        }
        this.unbind();
        this.d.find('.toolbar .rooted input').prop('checked', false); 
        // edit children
        var children = this.children;
        for (var i = 0 ; i < children.length; i++) {
            children[i].edit();
        }
        // children are not children any more
        this.for = this.children;
        this.children = [];
        return this;
    },

            
    update: function() {
        if (!this.arrow) {
            return this.confirm().update();
        }
        
        var self = this;
        
        var processPartners = function(list) {
            var d = self.d;
            var p = d.position();
            var source = self.arrow;
            if (!source) {
                console.log("View got CGed before processPartners");
                return;
            }
            console.log((self.terminal.entrelacs.uriIfKnown(source) || Arrow.serialize(source)) + " partners = {");
            while (list !== Arrow.eve) {
                var pair = list.getTail();
                console.log(Arrow.serialize(pair) + ",");
                var outgoing = (pair.getTail() == source); 
                var partner = (outgoing ? pair.getHead() : pair.getTail());
                var a = self.terminal.findNearestArrowView(partner, p, 1000);
                if (!a) {
                    var c = self.terminal.getPointOnCircle(50);
                    a = self.terminal.show(partner,
                            p.left + (outgoing ? 100 + c.x : -100 - c.x),
                            p.top + c.y);
                }
                var pairView = self.terminal.findNearestArrowView(pair, p, 1000);
                if (!pairView || pairView.tv !== (outgoing ? self: a) || pairView.hv !== (outgoing ? a : self)) {
                    pairView = outgoing ? new PairView(pair, self.terminal, self, a, pair) : new PairView(pair, self.terminal, a, self, pair);
                    pairView.restoreGeometry(a, pairView);
                } else {
                    pairView.d.css('border-bottom-width', '5px').animate({'border-bottom-width': 0});
                }

                var geoKey = Arrow.pair(Arrow.atom("geometry"), pair);
                (function(geoKey, pairView, a, pair) {
                    self.terminal.entrelacs.getPartners(geoKey).done(function() {
                        pairView.restoreGeometry(a, pairView);
                    });
                })(geoKey, pairView, a, pair);
            
                var next = list.getHead();
                if (next)
                    list = next;
                else { // list not completly unfolded. end with a placeholder
                    var promise = self.terminal.entrelacs.open(list);
                    promise.done(function(unfolded) {
                        processPartners(self, unfolded);
                    });
                    list = Arrow.eve;
                }
            }
            console.log("}");
        };

        self.terminal.moveLoadindBarOnView(self);
    
        var promise = self.terminal.entrelacs.isRooted(self.arrow);
        promise.always(function () {
            self.bar.find('.rooted input').prop('checked', self.arrow.isRooted());
        });

        // process local partners
        var futur = function() {
            var knownPartners = self.arrow.getPartners();
            self.d.css('border-bottom-width', '5px').animate({'border-bottom-width': 0});
            processPartners(knownPartners);
            return $.when(knownPartners);
        };
        self.terminal.entrelacs.chain = self.terminal.entrelacs.chain.pipe(futur, futur);
        
        // process remote partners
        promise = self.terminal.entrelacs.getPartners(self.arrow);
        promise.done(processPartners);
        return promise;
    },

    /** focus */
    focus: function () {
        return false;
    },
    
    /** close */
    close: function() {
        var self = this;
        self.d.fadeOut(200, function() { self.dismiss(); });
    },
    
    /** is pair view ? */
    isPairView: function() { return true; },
    
    getTerminal: function() { return this.terminal; },
    getArrow: function() {return this.arrow; },
    getElement: function() { return this.d[0]; }
};
