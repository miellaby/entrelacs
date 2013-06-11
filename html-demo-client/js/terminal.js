function Terminal(area, entrelacs, animatePlease) {
    this.animatePlease = animatePlease || false;
    this.area = area;
    this.entrelacs = entrelacs;
    this.area.data('terminal', this);
    area.click(this.on.area.click);
    
    this.toaster = $("<div id='toaster'>...</div>");
    this.toaster.appendTo(area);
    this.toasterHeight = this.toaster.height();
    this.toaster.hide();

    this.circleAngle = 0;
    this.uploadCount = 0;
}

Terminal.prototype = {

    addBarTo: function(d) {
        var h = $("<div class='hook'><div class='in'>&uarr;</div> <div class='rooted'><input type='checkbox' disabled></div> <div class='poke'>?</div>  <div class='out'>&darr;</div></div>");
        h.appendTo(d);
        h.hover(this.on.bar.enter, this.on.bar.leave);
        h.children('.in,.out,.poke,.rooted').click(this.on.bar.click);
        h.find('.rooted input').change(this.on.bar.rooted.change);
    },

    openPrompt: function(x, y, initialValue) {
        var d = $("<div class='atomDiv'><input type='text'></input><div class='close'><a href='#'>&times;</a></div></div>");
        this.area.append(d);
        d.css({ 'left': x + 'px',
                'top': y + 'px' });
        if (this.animatePlease) {
            d.css('opacity', 0.1).css('width', '10px').animate({ opacity: 1, left: "-=" + parseInt(defaultEntryWidth / 2) + "px", width: defaultEntryWidth}, 200, 'swing', function() { d.css('width', 'auto');});
        }
        
        var i = d.children('input');
        if (initialValue) i.val(initialValue);

        // set listeners
        i.blur(this.on.prompt.blur);
        i.click(this.on.prompt.click);
        i.focus(this.on.prompt.focus);
        i.keypress(this.on.prompt.keypress);
        i.keydown(this.on.prompt.keydown);
        d.attr('draggable', true);
        i.on('dragenter', this.on.arrow.dragenter);
        i.on('dragleave', this.on.arrow.dragleave);
        d.on('dragstart', this.on.arrow.dragstart);
        d.on('dragend', this.on.arrow.dragend);
        d.find('.close a').click(this.on.arrow.close.click);
        this.addBarTo(d);
        
        // data
        d.data('for', []);
        d.data('children', []);
        
        // file uploading
        d.append("<span class='fileinput-button'><span>...</span><input type='file' name='files[]'></span>");
        d.children('.fileinput-button').click(this.on.prompt.fileInputButton.click).change(this.on.prompt.fileInputButton.change);
        return d;
    },
    
    putFolded: function(x, y) {
        var d = $("<div class='pairDiv'><div class='tailDiv'><div class='tailEnd'></div></div><div class='headDiv'></div><button class='unfoldTail'>+</button><button class='unfoldHead'>+</button><div class='close'><a href='#'>&times;</a></div></div>");
        this.area.append(d);
        d.css({left: (x - 75) + 'px',
               top: (y - 45) + 'px',
               height: '45px',
               width: '150px'});
        d.children('.headDiv,.tailDiv').height(0).width(20).css('border-style', 'dashed');

        // set listeners
        d.attr('draggable', true);
        d.on('dragstart', this.on.dragstart);
        d.on('dragend', this.on.dragend);
        d.find('.close a').click(this.on.close.click);
        d.children('.unfoldTail,.unfoldHead').click(this.on.unfold.click);
        
        this.addBarTo(d);
        
        // data
        d.data('for', []);
        d.data('children', []);
        return d;
    },

    isPrompt: function(d) {
        return d.hasClass('atomDiv');
    },
    
    isPair: function(d) {
        return d.hasClass('pairDiv') || d.hasClass('ipairDiv');
    },
    
    removeFor: function(d, a) {
        var list = d.data('for');
        for (var i = 0; i < list.length; i++) {
           var c = list[i];
           if  (c[0] === a[0]) {
               list.splice(i, 1);
               break;
           }
        }
        return list;
    },

    
    removeChild: function(d, a) {
        var list = d.data('children');
        for (var i = 0; i < list.length; i++) {
           var c = list[i];
           if  (c[0] === a[0]) {
               list.splice(i, 1);
               break;
           }
        }
        return list;
    },

    isArrowLoose: function(d) {
        return (!d.data('for').length && !d.data('children').length && !d.data('arrow'));
    },

    dismissArrow: function(d, direction /* false = down true = up */) {
        // guard double detach
        if (d.data('detached')) return;
        // for guard
        d.data('detached', true);

        if (d.hasClass('pairDiv') || d.hasClass('ipairDiv')) { // pair
            // propagate deletion to tail
            var t = d.data('tail');
            if (t) {
                this.removeFor(t, d);
                this.removeChild(t, d);
                if (this.isArrowLoose(t))
                   this.dismissArrow(t, true);
            }

            // propagate deletion to head
            var h = d.data('head');
            if (h) {
                this.removeFor(h, d);
                this.removeChild(h, d);
                if (this.isArrowLoose(h))
                   this.dismissArrow(h, true);
            }

        }

        // propagate to loose child
        if (!direction) { //  when going "down"
            var f = d.data('for'); 
            while (f.length) {
                this.dismissArrow(f[0]);
            }
        }

        // also fold children
        var children = d.data('children');
        for (var i = 0; i < children.length; i++) {
            var c = children[i];
            // var subchildren = c.data('children');
            // TODO show unfold buttons
            if (c.data('head') && c.data('head')[0] === d[0]) {
                c.children('.headDiv').height(0).width(20).css('border-style', 'dashed');
                u = $("<button class='unfoldHead'>+</button>");
                u.click(this.on.unfold.click);
                u.appendTo(c);
                c.removeData('head');
                continue;
            }
            if (c.data('tail') && c.data('tail')[0] === d[0]) {
                c.children('.tailDiv').height(0).width(20).css('border-style', 'dashed').children('.tailEnd').hide();
                u = $("<button class='unfoldTail'>+</button>");
                u.click(this.on.unfold.click);
                u.appendTo(c);
                c.removeData('tail');
                continue;
            }
        }

        d.detach();
    },

    moveArrow: function(d, offsetX, offsetY, movingChild) {
        var p = d.position();
        d.css({ 'opacity': 1,
                'left': (p.left  + offsetX) + 'px',
                'top': (p.top + offsetY) +'px'
        });
        if (d.hasClass('pairDiv') ||d.hasClass('ipairDiv')) {
           if (d.data('head')) {
               this.moveArrow(d.data('head'), offsetX, offsetY, d);
           }
           if (d.data('tail') && !(d.data('head') && d.data('tail')[0] === d.data('head')[0])) {
               this.moveArrow(d.data('tail'), offsetX, offsetY, d);
           }
        }
        this.updateDescendants(d, d);
    },

    updateDescendants: function(a, newA) {
        var f = a.data('for');
        for (var i = 0; i < f.length; i++) { // search into loose children
            this.rewirePair(f[i], a, newA);
        }
        var children = a.data('children');
        for (var i = 0; i < children.length; i++) {
            this.rewirePair(children[i], a, newA);
        }
    },

    updateChild: function(d, a, newA) {
        var list = d.data('children');
        for (var i = 0; i < list.length; i++) {
           var c = list[i];
           if  (c[0] === a[0]) {
               list.splice(i, 1, newA);
               break;
           }
        }
        return list;
    },

    updateFor: function(d, a, newA) {
        var list = d.data('for');
        for (var i = 0; i < list.length; i++) {
           var c = list[i];
           if  (c[0] === a[0]) {
               list.splice(i, 1, newA);
               break;
           }
        }
        return list;
    },

    updateRefs: function(d, oldOne, newOne) {
        this.updateFor(d, oldOne, newOne);
        this.updateChild(d, oldOne, newOne);
    },


    rewirePair: function(a, oldOne, newOne) {
       var oldTail = a.data('tail');
       var oldHead = a.data('head');

       var tailChanged = (oldTail && oldTail[0] === oldOne[0]);
       var headChanged = (oldHead && oldHead[0] === oldOne[0]);

       var newTail = tailChanged ? newOne : oldTail;
       var newHead = headChanged ? newOne : oldHead;
       var newA = this.pairTogether(newTail, newHead);
       newA.find('.hook .poke').text(a.find('.hook .poke').text());

       // props copy
       newA.find('.hook .rooted input').prop('disabled', a.find('.hook .rooted input').prop('disabled'));
       newA.find('.hook .rooted input').prop('checked', a.find('.hook .rooted input').prop('checked'));
       
       // data copy
       var oldData = a.data();
       for (var dataKey in oldData) {
          if (dataKey == 'head' || dataKey == 'tail') continue;
          newA.data(dataKey, oldData[dataKey]);
       }
       // back refs updating
       if (newTail)
           this.updateRefs(newTail, a, newA);
           
       if (newHead && !(newTail && newTail[0] === newHead[0])) {
          this.updateRefs(newHead, a, newA);
       }

       this.updateDescendants(a, newA);
       
       a.detach();

       return newA;
    },
    
    closePrompt: function(prompt) {
        this.dismissArrow(prompt);
        this.entrelacs.commit();
    },
    
    closePromptIfLoose: function(prompt) {
          if (this.isArrowLoose(prompt)) {
            var self = this;
            setTimeout(function() {
                if (!prompt.children('input,textarea').is(':focus')
                    && self.isArrowLoose(prompt))
                     self.closePrompt(prompt);
            }, 1000);
        }
    },

    enlargePrompt: function(prompt) {
        var w0 = prompt.width();
        var i = prompt.children('input');
        var v = i.val();
        
        i.detach();
        i = $("<textarea></textarea>").text(v).appendTo(promt);
        i.blur(this.on.entry.blur);
        i.click(this.on.atom.click);
        i.focus(this.on.entry.focus);
        /* i.keypress(onEntryKeypress); no 13 catch */
        i.keydown(this.on.entry.keydown);
        i.on('dragenter', this.on.entry.dragenter);
        i.on('dragleave', this.on.entry.dragleave);
        i.focus();
        var w = i.width();
        prompt.css('left', (w < w0 ? '+=' + ((w0 - w) / 4) : '-=' + ((w - w0) / 4)) + 'px');
    },

    turnPromptIntoArrow: function(prompt) {
        var w0 = prompt.width();
        var arrow = prompt.data('arrow');
        var body = arrow.getBody();
        prompt.addClass('kept');
        prompt.children('.close').show();
        prompt.children('input,textarea').detach();
        var s = $("<span></span>").text(body || 'EVE').appendTo(prompt);
        s.click(this.on.atom.click);
        var w = s.width();
        s.css({width: (w < 100 ? 100 : w) + 'px', 'text-align': 'center'});
        prompt.css('left', (w < w0 ? '+=' + ((w0 - w) / 4) : '-=' + ((w - w0) / 4)) + 'px');
        prompt.children('.fileinput-button').detach();
        prompt.find('.hook .rooted input').prop('disabled', false).prop('checked', arrow.isRooted());

    },

    processPartners: function(d, list) {
        var p = d.position();
        var source = d.data('arrow');

        while (list !== Arrow.eve) {
            var link = list.getTail();
            var outgoing = (link.getTail() == source); 
            var partner = (outgoing ? link.getHead() : link.getTail());
            var c = this.getPointOnCircle(50);
            var a;
            if (partner.isAtomic()) {
                a = this.openPrompt(p.left + (outgoing ? d.width() + 100 + c.x : -100 - defaultEntryWidth - c.x),
                                   p.top + d.height() / 2 + c.y,
                                   partner.getBody());
            } else if (partner.head === undefined) { // placeholder
                a = this.addFolded(p.left + (outgoing ? 3 : -3 - d.width()), p.top);
            } else {
                // TODO pair
            }
            var pair = outgoing ? pairTogether(d, a) : pairTogether(a, d);
            d.data('children').push(pair);
            a.data('children').push(pair);
            this.getArrow(pair);
            
            var next = list.getHead();
            if (next)
                list = next;
            else { // list not completly unfolded. end with a placeholder
            
                var promise = this.entrelacs.open(list);
                promise.done(function(unfolded) {
                    self.processPartners(unfolded);
                });
                list = Arrow.eve;
            }
                
        }
    },
    
    update: function(d) {
        var arrow = d.data('arrow');
        var self = this;
        d.css('marginTop','-5px').animate({marginTop: 0});
        var promise = self.entrelacs.isRooted(arrow);
        promise.done(function () {
            d.find('.hook .rooted input').prop('checked', arrow.isRooted()).prop('disabled', false);
            var knownPartners = arrow.getPartners();
            self.processPartners(d, knownPartners);
            var promise = self.entrelacs.getPartners(arrow);
            promise.done(function(partners) {
                self.processPartners(d, partners);
            });
        });
        return promise;
    },
    
    getArrow: function(d) {
        var arrow = d.data('arrow');
        if (arrow) return arrow;
        
        if (this.isPrompt(d)) {
            var string = d.children('input').val();
            arrow = Arrow.atom(string);
            d.data('arrow', arrow);
            this.turnPromptIntoArrow(d);
        } else {
            var from = this.getArrow(d.data('tail'));
            var to = this.getArrow(d.data('head'));
            arrow = Arrow.pair(from, to);
        }
        d.data('arrow', arrow);
        var views = arrow.get('views');
        if (views === undefined) {
            views = [];
            arrow.set('views', views);
        }
        views.push(d);
        return arrow;
    },
    
    submitArrows: function(arrow) {
        var f = arrow.data('for');
        if (f.length) { // get back to the roots (recursive calls to all waiting prompt arrows)
            var ff;
            for (var i = 0 ; i < f.length; i++) {
                toBeRecorded = f[i];
                this.submitArrows(toBeRecorded);
            }
        } else { // terminal case: arrow to be recorded
            this.getArrow(arrow);
            this.update(arrow);
        }
    },

    pairTogether: function(tail, head) {
        // TODO fix default position if head or tail not set
        var p0 = tail ? tail.position() : head.position();
        var p1 = head ? head.position() : tail.position();
        var x0 = p0.left + (tail ? tail.width() : defaultEntryWidth) / 2 + 3;
        var y0 = p0.top + (tail ? tail.height() : defaultEntryHeight);
        var x1 = p1.left + (head ? head.width() : defaultEntryWidth) / 2 - 3;
        var y1 = p1.top + (head ? head.height() : defaultEntryHeight);
        var d = $("<div class='" + (x0 < x1 ? "pairDiv" : "ipairDiv") + "'><div class='tailDiv'><div class='tailEnd'></div></div><div class='headDiv'></div><div class='close'><a href='#'>&times;</a></div></div>");
        this.area.append(d);
        var marge = Math.max(20, Math.min(50, Math.abs(y1 - y0)));
        d.css({
            'left': Math.min(x0, x1) + 'px',
            'top': Math.min(y0, y1) + 'px',
            'width': Math.abs(x1 - x0) + 'px',
            'height': (Math.abs(y1 - y0) + marge) + 'px'
        });
        if (this.pleaseAnimate) {
            d.hide();
            d.children('.tailDiv').animate({'height': (marge + ((y0 < y1) ? y1 - y0 : 0)) + 'px'});
            d.children('.headDiv').animate({'height': (marge + ((y1 < y0) ? y0 - y1 : 0)) + 'px'});
            d.fadeIn(500);
        } else {
            d.children('.tailDiv').css({'height': (marge + ((y0 < y1) ? y1 - y0 : 0)) + 'px'});
            d.children('.headDiv').css({'height': (marge + ((y1 < y0) ? y0 - y1 : 0)) + 'px'});
        }
        
        // set listeners
        d.attr('draggable', true);
        d.on('dragstart', this.on.arrow.dragstart);
        d.on('dragend', this.on.arrow.dragend);
        d.find('.close a').click(this.on.arrow.close.click);

        this.addBarTo(d);
        
        // data
        if (tail) {
            d.data('tail', tail);
        } else {
            d.children('.tailDiv').height(0).width(20).css('border-style', 'dashed');
            u = $("<button class='unfoldTail'>+</button>");
            u.click(unfoldClick);
            u.appendTo(d);
        }

        if (head) {
            d.data('head', head);
        } else {
            d.children('.headDiv').height(0).width(20).css('border-style', 'dashed');
            u = $("<button class='unfoldHead'>+</button>");
            u.click(unfoldClick);
            u.appendTo(d);
        }

        d.data('for', []);
        d.data('children', []);
        return d;

    },
    
    getPointOnCircle: function(radius) {
        this.circleAngle += 4;
        var x = radius * Math.sin(2 * Math.PI * this.circleAngle / 23);
        var y = radius * Math.cos(2 * Math.PI * this.circleAngle / 23);
        var p = { x: parseInt(x), y: parseInt(y)};
        return p;
    },

    leave: function(d, incoming) {
           var p = d.position();
           var c = this.getPointOnCircle(100);
           var n, a;
           if (incoming) {
              n = this.openPrompt(p.left - defaultEntryWidth - 100 - c.x, p.top + d.height() / 2 - c.y);
              a = this.pairTogether(n, d);
           } else {
              n = this.openPrompt(p.left + d.width() + 100 + c.x, p.top + d.height() / 2 + c.y);
              a = this.pairTogether(d, n);
           }
           n.data('for').push(a);
           n.children("input").focus();
           return a;

    },

    // find an editable atom into a tree
    findNextInto: function(d) {
        if (d.hasClass('atomDiv')) { // atom
            return d.data('for').length ? d /* found */: null;
        } else { // pair
           // check in tail then in head
           var n = d.data('tail') ? this.findNextInto(d.data('tail')) : null;
           if (!n && d.data('head'))
                n = this.findNextInto(d.data('head'));
           return n;
        }
    },

    // search for the next editable atom
    findNext: function(d, from, leftOnly) {
       var f = d.data('for');
       if ((d.hasClass('pairDiv') || d.hasClass('ipairDiv')) && d.data('head') && d.data('head')[0] !== from[0]) { // search into head
          var n = this.findNextInto(d.data('head'));
          if (n) return n;
       }
       
       for (var i = 0; i < f.length; i++) { // search into loose children
            var n = this.findNext(f[i], d, leftOnly);
            if (n) return n;
       }
       
      if (!leftOnly && d.hasClass('pairDiv') && d.data('tail') && d.data('tail')[0] !== from[0]) { // search into tail
          var n = this.findNextInto(d.data('tail'));
          if (n) return n;
       }

       // search fail: no editable
       return null;
    },
    
    // find previous editable atom into a tree
    findPreviousInto: function(d) {
        if (d.hasClass('atomDiv')) { // atom
            return d.data('for').length ? d /* found */: null;
        } else { // pair
           // check in head then in tail
           var n = d.data('head') ? this.findPreviousInto(d.data('head')) : null;
           if (!n && d.data('tail'))
                n = this.findPreviousInto(d.data('tail'));
           return n;
        }
    },
    

    // search for the previous editable atom
    findPrevious: function(d, from, rightOnly) {
       var f = d.data('for');
       if ((d.hasClass('pairDiv') || d.hasClass('ipairDiv')) && d.data('tail') && d.data('tail')[0] !== from[0]) { // search into tail
          var n = this.findPreviousInto(d.data('tail'));
          if (n) return n;
       }
       
       for (var i = 0; i < f.length; i++) { // search into loose children
            var n = this.findPrevious(f[i], d, rightOnly);
            if (n) return n;
       }
       
      if (!rightOnly && d.hasClass('pairDiv') && d.data('head') && d.data('head')[0] !== from[0]) { // search into tail
          var n = this.findPreviousInto(d.data('head'));
          if (n) return n;
       }

       // search fail: no editable
       return null;
    },

    
    on:{
        area: {
            click: function(event) {
                
                var self = $(this).data('terminal');
                var x = event.pageX;
                var y = event.pageY;
                self.openPrompt(x, y, '', this.animatePlease).children('input').focus();
            }
        },
        bar: {
            click: function(event) {
                var button = $(this);
                var d = button.parent().parent();
                var self = d.parent().data('terminal');
                var a;
                if (button.hasClass('.rooted')) {
                   return false;
                }

                if (button.hasClass('poke')) {
                   var arrow = d.data('arrow');
                   if (arrow) {
                        self.update(d);
                   }
                   
                   return false;
                }

                if (button.hasClass('in'))
                    a = self.leave(d, true);
                else if (button.hasClass('out'))
                    a = self.leave(d, false);
                
                if (a)
                   d.data('for').push(a);
                return false;
            },
            rooted: {
                change: function(event) {
                    var d  = $(this).parent().parent().parent();
                    var self = d.parent().data('terminal');
                    var arrow = d.data('arrow');
                    if (arrow) {
                        if (arrow.isRooted()) {
                            arrow.unroot();
                        } else {
                            arrow.root();
                        }
                        Arrow.commit(self.entrelacs);
                    }
                    return true;
                },
            },
            enter: function(event) {
                var bar = $(this);
                bar.children('.in,.out,.rooted,.poke').show().css({'height': '0px', 'opacity': 0}).animate({'height': '20px', opacity: 1}, 50);
            },
            leave: function(event) {
                var bar = $(this);
                bar.children('.in,.out,.rooted,.poke').css({'height': '20px', 'opacity': 1}).animate({'height': '0px', opacity: 0}, 200);
            }
        },
        prompt: {
            blur: function(event) {
                var prompt = $(this).parent();
                var area = prompt.parent();
                var self = area.data('terminal');
                self.closePromptIfLoose(prompt);
            },
            click: function(e) {
                var prompt = $(this).parent();
                var self = prompt.parent().data('terminal');
                return false;
            },
            keypress: function(event) {
                if (event.which == 13 /* enter */) {
                    var prompt = $(this).parent();
                    var area = prompt.parent();
                    var self = area.data('terminal');
                    event.preventDefault();
                    // record arrows that this prompt belongs too
                    self.submitArrows(prompt);
                    Arrow.commit(self.entrelacs);

                    return false;
                }
                return true;

            },
            keydown: function(event) {
                if (event.ctrlKey && event.keyCode == 13 /* Return */) {
                    var prompt = $(this).parent();
                    var area = prompt.parent();
                    var self = area.data('terminal');

                    if ($(this).is('textarea')) {
                        // record the currently edited compound arrow (or at least this text input)
                        self.submitArrows(prompt);
                        Arrow.commit(self.entrelacs);
                    } else {
                        // turn the atom into a textarea
                        self.enlargePrompt(prompt);
                    }
                    event.preventDefault();
                    return false;
                } else if (event.keyCode == 27 /* escape */) {
                    var prompt = $(this).parent();
                    var area = prompt.parent();
                    var self = area.data('terminal');
                    event.preventDefault();
                    self.closePrompt(prompt);
                    return false;
                } else if (event.keyCode == 9 /* tab */) {
                    var prompt = $(this).parent();
                    var area = prompt.parent();
                    var self = area.data('terminal');
                    event.preventDefault();
                    
                    var f = prompt.data('for');
                    for (var i = 0; i < f.length; i++) { // search into loose children
                        var next = event.shiftKey ? self.findPrevious(f[i], prompt) : self.findNext(f[i], prompt);
                        if (next) {
                            next.children('input,textarea').focus();
                            return;
                        }
                    }

                    if (f.length) {
                        var ff;
                        while ((ff = f[f.length - 1].data('for')) && ff.length) {
                            f = ff ;
                        }
                        var a = self.leave(f[f.length - 1], event.shiftKey /* in/out */);
                        f.data('for').push(a);
                    } else {
                        var a = self.leave(prompt, event.shiftKey /* out */);
                        prompt.data('for').push(a);
                    }
                }

                return true;

            },

            fileInputButton: {
                click: function(event) {
                },
                change: function(event) {
                }
            },
        },
        arrow: {
            dragenter: function(event) {
                var prompt = $(this).parent();
                var area = prompt.parent();
                var self = area.data('terminal');
                self.dragOver = this;
                $(this).animate({'border-width': 6, left: "-=3px"}, 100);
                event.preventDefault();
                event.stopPropagation();
                return false;
            },
            dragleave: function(event) {
                var prompt = $(this).parent();
                var area = prompt.parent();
                var self = area.data('terminal');
                $(this).animate({'border-width': 2, left: "+=3px"}, 100);
                setTimeout(function() { self.dragOver = null; }, 0); // my gosh
                event.preventDefault();
                return false;
            },
            dragstart: function(event) {
                var prompt = $(this);
                var area = prompt.parent();
                var self = area.data('terminal');
                self.dragStartX = Math.max(event.originalEvent.screenX, event.originalEvent.pageX);
                self.dragStartY = Math.max(event.originalEvent.screenY, event.originalEvent.pageY);
                event.originalEvent.dataTransfer.setData('text/plain', "arrow");
                event.originalEvent.dataTransfer.effectAllowed = "move";
                event.originalEvent.dataTransfer.dropEffect = "move";
            },
            dragend: function(event) {
                var arrow = $(this);
                var area = arrow.parent();
                var self = area.data('terminal');
                if (event.originalEvent.dropEffect == "none") return false;
    
                if (self.dragOver && arrow.data('arrow')) {
                    var d = $(self.dragOver).parent();
                    self.turnPromptIntoExistingArrow(d, $(this));
                    self.submitArrow(d); // TODO Need a confirm button
                } else {
                    self.moveArrow(arrow, Math.max(event.originalEvent.screenX, event.originalEvent.pageX) - self.dragStartX,
                                 Math.max(event.originalEvent.screenY, event.originalEvent.pageY) - self.dragStartY, null /* no moving child */);
                }
                return true;
            },
            close: {
                click: function(event) {
                }
            },
        },
        atom: {
            click: function(event) {
                return false;
            },
        },
        unfold: {
            click: function(event) {
            },
        },
    },
    
    
};

function init() {
    var area = $('#area');
    var entrelacs = new Entrelacs();
    var terminal = new Terminal(area, entrelacs, true);
    $(document).scrollTop(area.height() / 2 - $(window).height() / 2);
    $(document).scrollLeft(area.width() / 2 - $(window).width() / 2);
    defaultEntryWidth = $('#proto_entry').width();
    defaultEntryHeight = $('#proto_entry').height();
    $('#proto_entry').detach();
    setTimeout(function() { $("#killme").fadeOut(4000, function(){$(this).detach();}); }, 200);

    $(document).ajaxStart(function(){
        $("#loading").show();
    }).ajaxStop(function(){
        if (!terminal.uploadCount) $("#loading").hide();
    });
    
    creole = new Parse.Simple.Creole({forIE: document.all,
                                     interwiki: {
                WikiCreole: 'http://www.wikicreole.org/wiki/',
                Wikipedia: 'http://en.wikipedia.org/wiki/'},
                                     linkFormat: '' });
                                     
    //findFeaturedArrows();
}


$(document).ready(init);
