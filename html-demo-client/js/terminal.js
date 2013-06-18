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

    var self = this;
    Arrow.listeners.push(function(a) { self.arrowEvent(a); });
}

Terminal.prototype = {

    addBarTo: function(d) {
        var h = $("<div class='hook'><div class='in'>&uarr;</div> <div class='rooted'><input type='checkbox' disabled></div> <div class='poke'>?</div> <div class='close'>&times;</div> <div class='out'>&darr;</div></div>");
        h.appendTo(d);
        h.hover(this.on.bar.enter, this.on.bar.leave);
        h.children('div').click(this.on.bar.click);
        h.find('.rooted input').change(this.on.bar.rooted.change);
        h.click(this.on.bar.click);
    },

    bindViewToArrow: function(view, arrow) {
        view.data('arrow', arrow);
        var views = arrow.get('views');
        if (views === undefined) {
            views = [];
            arrow.set('views', views);
        }
        views.push(view[0]);
        view.find('.hook .rooted input').prop('disabled', false).prop('checked', arrow.isRooted());
    },
    
    putAtomView: function(x, y, atom) {
        var d = $("<div class='atomDiv'><span class='content'></span><div class='close'><a href='#'>&times;</a></div></div>");
        this.area.append(d);
        d.css({ 'left': x + 'px',
                'top': y + 'px' });
        if (this.animatePlease) {
            d.css('opacity', 0.1).animate({ opacity: 1}, 200, 'swing');
        }
        
        var s = d.children('span');
        s.text(atom.getBody() || 'EVE');
        var w = Math.max(s.width(), 100);
        s.css({width: '' + w + 'px', 'text-align': 'center'});
        d.css('left', '-=' + Math.round(w / 2, 0) + 'px');

        d.attr('draggable', true);
        d.on('dragstart', this.on.view.dragstart);
        d.on('dragend', this.on.view.dragend);
        d.find('.close a').click(this.on.view.close.click);

        this.addBarTo(d);
        
        // data
        d.data('for', []);
        d.data('children', []);
        this.bindViewToArrow(d, atom);
        
        d.children('.close').show();
        return d;
    },
    
    putPrompt: function(x, y, initialValue) {
        var d = $("<div class='atomDiv'><input type='text'></input><div class='close'><a href='#'>&times;</a></div></div>");
        this.area.append(d);
        d.css({ 'left': x + 'px',
                'top': y + 'px' });
        if (this.animatePlease && initialValue === undefined) {
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
        i.on('dragenter', this.on.view.dragenter);
        i.on('dragleave', this.on.view.dragleave);
        d.on('dragstart', this.on.view.dragstart);
        d.on('dragend', this.on.view.dragend);
        d.find('.close a').click(this.on.view.close.click);
        this.addBarTo(d);
        
        // data
        d.data('for', []);
        d.data('children', []);
        
        // split button
        d.prepend("<a class='split' style='display: block; position: absolute; right: 0px; margin-right: -20px;'>||</a>");
        d.children('.split').click(this.on.prompt.split.click);

        // file uploading
        d.append("<span class='fileinput-button'><span>...</span><input type='file' name='files[]'></span>");
        d.children('.fileinput-button').click(this.on.prompt.fileInputButton.click).change(this.on.prompt.fileInputButton.change);
        

        return d;
    },
    
    turnPromptIntoAtomView: function(prompt) {
        var w0 = prompt.width();
        var body = prompt.children('input,textarea').val();
        prompt.addClass('kept');
        prompt.children('.close').show();
        prompt.children('input,textarea').detach();
        var s = $("<span class='content'></span>").text(body || 'EVE').appendTo(prompt);
        var w = s.width();
        s.css({width: (w < 100 ? 100 : w) + 'px', 'text-align': 'center'});
        prompt.css('left', (w < w0 ? '+=' + ((w0 - w) / 4) : '-=' + ((w - w0) / 4)) + 'px');
        prompt.children('.fileinput-button,.split').detach();
        prompt.find('.hook .rooted input').prop('disabled', false);
    },

    splitPrompt: function(prompt) {
        var v = prompt.children('input,textarea').val();
        var p = prompt.position();
        var width = prompt.width();
        var height = prompt.height();

        // peek a counting circle point
        var c = this.getPointOnCircle(50);
        
        // build a tail prompt
        var t = this.putPrompt(p.left - width / 2 - 50 - c.x, p.top + height / 2 - 100 - c.y, v);

        // build a head prompt
        var h = this.putPrompt(p.left + width / 2 + 50 + c.x, p.top + height / 2 - 50 + c.y);

        // pair head and tail as loose arrows
        var a = this.pairTogether(t, h);
        t.data('for').push(a);
        h.data('for').push(a);

        // replace prompt with pair
        this.replaceView(prompt, a);


        // detach old prompt
        prompt.detach();

        // focus on tail prompt
        t.children('input').focus();
    },
    
    
    putPlaceholder: function(x, y, arrow) {
        var d = $("<div class='placeholderDiv'><button class='unfold'>+</button></div>");
        this.area.append(d);
        d.css({left: x - 12 + 'px',
               top: y - 12 + 'px'});
               
        // set listeners
        d.attr('draggable', true);
        d.on('dragstart', this.on.dragstart);
        d.on('dragend', this.on.dragend);
        d.find('.close a').click(this.on.view.close.click);
        d.children('.unfold').click(this.on.unfold.click);
        
        // data
        d.data('for', []);
        d.data('children', []);
        this.bindViewToArrow(d, arrow);

        return d;
    },

    unfold: function(placeholder) {
        var self = this;
        var arrow = placeholder.data('arrow');
        var p = placeholder.position();
         
        if (arrow.isAtomic() === undefined) { // a placeholder
            var promise = this.entrelacs.open(arrow);
            promise.done(function(unfolded) {
                var vs = arrow.get('views');
                vs.splice(vs.indexOf(placeholder[0]), 1);

                var a = self.putArrowView(p.left, p.top, unfolded);
                // rewire children and prompt trees
                self.replaceView(placeholder, a);
                placeholder.detach();

            });
            return;
        }
        
        var vs = arrow.get('views');
        vs.splice(vs.indexOf(placeholder[0]), 1);
        var a = self.putArrowView(p.left, p.top, arrow);

        // rewire children and prompt trees
        this.replaceView(placeholder, a);
        
        // note: detach element after .data() access
        placeholder.detach();
    },
    
    putArrowView: function(x, y, arrow) {
        var a = this.findNearestArrowView(arrow, {left: x, top: y}, 1000);
        if (a) return a;
        if (arrow.isAtomic()) {
            a = this.putAtomView(x, y, arrow);
        } else if (arrow.isAtomic() === undefined) { // placeholder
            a = this.putPlaceholder(x, y, arrow);
        } else {
            var t = this.putArrowView(x - 100 - defaultEntryWidth, y - 170, arrow.getTail());
            var h = this.putArrowView(x + 100, y - 130, arrow.getHead());
            a = this.pairTogether(t, h);
            t.data('children').push(a);
            h.data('children').push(a);
            this.bindViewToArrow(a, arrow);
        }

        return a;
    },

    isAtomView: function(d) {
        return d.hasClass('atomDiv');
    },
    
    isPairView: function(d) {
        return d.hasClass('pairDiv') || d.hasClass('ipairDiv');
    },
    
    removeFor: function(d) {
        [d.data('tail'), d.data('head')].forEach(function(end) {
            if (!end) return;
            var list = end.data('for');
            for (var i = 0; i < list.length; i++) {
                var c = list[i];
                if  (c[0] === d[0]) {
                    list.splice(i, 1);
                    break;
                }
            }
        });
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

    isViewLoose: function(d) {
        // Check view is in no tree
        if (d.data('for').length || d.data('children').length)
            return false;
        // Check arrow is not rooted
        var arrow = d.data('arrow');
        return (!arrow || !arrow.isRooted());
    },

    dismissView: function(d, direction /* false = down true = up */) {
        // guard double detach
        if (d.data('detached')) return;
        // for guard
        d.data('detached', true);

        if (d.hasClass('pairDiv') || d.hasClass('ipairDiv')) { // pair
            this.removeFor(d);

            // propagate deletion to tail
            var t = d.data('tail');
            if (t) {
                this.removeChild(t, d);
                if (this.isViewLoose(t))
                   this.dismissView(t, true);
            }

            // propagate deletion to head
            var h = d.data('head');
            if (h) {
                this.removeChild(h, d);
                if (this.isViewLoose(h))
                   this.dismissView(h, true);
            }

        }

        // propagate to loose "prompt trees"
        if (!direction) { //  when going "down"
            var f = d.data('for'); 
            while (f.length) {
                this.dismissView(f[0]);
            }
        }

        // also fold children
        var children = d.data('children');
        if (children.length) {
            var p = d.position();
            var arrow = d.data('arrow');
            var placeholder = this.putPlaceholder(p.left + d.width() / 2, p.top + d.height(), arrow);
            this.replaceView(d, placeholder);
        }

        var arrow = d.data('arrow');
        if (arrow) {
            var vs = arrow.get('views');
            vs.splice(vs.indexOf(d[0]), 1);
        }

        d.detach();
    },

    moveView: function(d, offsetX, offsetY, movingChild) {
        var p = d.position();
        d.css({ 'opacity': 1,
                'left': (p.left  + offsetX) + 'px',
                'top': (p.top + offsetY) +'px'
        });
        if (d.hasClass('pairDiv') ||d.hasClass('ipairDiv')) {
           if (d.data('head')) {
               this.moveView(d.data('head'), offsetX, offsetY, d);
           }
           if (d.data('tail') && !(d.data('head') && d.data('tail')[0] === d.data('head')[0])) {
               this.moveView(d.data('tail'), offsetX, offsetY, d);
           }
        }
        if (!movingChild)
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

    replaceView: function(a, newA) {
       // descendants copy
       var oldData = a.data();
       newA.data('for', newA.data('for').concat(oldData['for']));
       newA.data('children', newA.data('children').concat(oldData['children']));
       this.updateDescendants(a, newA);
    },

    rewirePair: function(a, oldOne, newOne) {
       var oldTail = a.data('tail');
       var oldHead = a.data('head');
       var arrow = a.data('arrow');

       var tailChanged = (oldTail[0] === oldOne[0]);
       var headChanged = (oldHead[0] === oldOne[0]);

       var newTail = tailChanged ? newOne : oldTail;
       var newHead = headChanged ? newOne : oldHead;
       var newA = this.pairTogether(newTail, newHead, true /* immediate */);

       // back refs updating
       this.updateRefs(newTail, a, newA);
       if (newTail[0] !== newHead[0]) {
          this.updateRefs(newHead, a, newA);
       }

       // descendants copy and update
       this.replaceView(a, newA);

       if (arrow) {
            var vs = arrow.get('views');
            vs.splice(vs.indexOf(a[0]), 1);
            this.bindViewToArrow(newA, arrow);
       }
       a.detach();
       return newA;
    },
    
    closePrompt: function(prompt) {
        this.dismissView(prompt);
    },
    
    closePromptIfLoose: function(prompt) {
          if (this.isViewLoose(prompt)) {
            var self = this;
            setTimeout(function() {
                if (!prompt.children('input,textarea').is(':focus')
                    && self.isViewLoose(prompt) && !prompt.data('arrow'))
                     self.closePrompt(prompt);
            }, 1000);
        }
    },

    enlargePrompt: function(prompt) {
        var w0 = prompt.width();
        var i = prompt.children('input');
        var v = i.val();
        prompt.children('.fileinput-button').detach();
        var textPosition = i[0].selectionStart;
        i.detach();
        i = $("<textarea></textarea>").text(v).appendTo(prompt);
        i[0].setSelectionRange(textPosition, textPosition);
        i.blur(this.on.prompt.blur);
        i.focus(this.on.prompt.focus);
        /* i.keypress(onEntryKeypress); no 13 catch */
        i.keydown(this.on.prompt.keydown);
        i.click(this.on.prompt.click);
        i.on('dragenter', this.on.prompt.dragenter);
        i.on('dragleave', this.on.prompt.dragleave);
        i.focus();
        var w = i.width();
        prompt.css('left', (w < w0 ? '+=' + ((w0 - w) / 4) : '-=' + ((w - w0) / 4)) + 'px');
    },

    turnPromptIntoExistingView: function(prompt, a) {
        this.replaceView(prompt, a);
        
        // detach entry-atom
        prompt.detach();
    },


    processPartners: function(d, list) {
        var p = d.position();
        var source = d.data('arrow');

        while (list !== Arrow.eve) {
            var link = list.getTail();
            var outgoing = (link.getTail() == source); 
            var partner = (outgoing ? link.getHead() : link.getTail());
            var a = this.findNearestArrowView(partner, p, 1000);
            if (!a) {
                var c = this.getPointOnCircle(50);
                a = this.putArrowView(
                        p.left + (outgoing ? d.width() + 100 + c.x : -100 - defaultEntryWidth - c.x),
                        p.top + d.height() / 2 + c.y,
                        partner);
            }
            var pair = outgoing ? Arrow.pair(source, partner) : Arrow.pair(partner, source);
            var dPair = this.findNearestArrowView(pair, p, 1000);
            if (!dPair || dPair.data('tail')[0] !== (outgoing ? d : a)[0] || dPair.data('head')[0] !== (outgoing ? a : d)[0]) {
                dPair = outgoing ? this.pairTogether(d, a) : this.pairTogether(a, d);
                d.data('children').push(dPair);
                a.data('children').push(dPair);
                this.bindViewToArrow(dPair, pair);
            } else {
                dPair.css('marginTop','-5px').animate({marginTop: 0});
            }
            
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
        if (!arrow) arrow = this.getArrow(d);
        var self = this;
        d.css('marginTop','-5px').animate({marginTop: 0});
        this.moveLoadingOnArrow(d);
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
        
        if (this.isAtomView(d)) { // prompt
            var string = d.children('input').val();
            this.turnPromptIntoAtomView(d);
            arrow = Arrow.atom(string);
        } else {
            var from = this.getArrow(d.data('tail'));
            var to = this.getArrow(d.data('head'));
            arrow = Arrow.pair(from, to);
            d.data('tail').data('children').push(d);
            d.data('head').data('children').push(d);

        }
        this.bindViewToArrow(d, arrow);
        return arrow;
    },
    
    findNearestArrowView: function(arrow, position, limit) {
        var views = arrow.get('views');
        if (views === undefined || !views.length) return null;
        var distance = -1;
        views.forEach(function(view) {
            var vp = $(view).position();
            var vd = Math.abs(vp.left - position.left) + Math.abs(vp.top - position.top);
            if (distance == -1 || vd < distance) {
                distance = vd;
                nearest = view;
            }
        });
        
        return (limit !== undefined && distance > limit ? null : $(nearest));
    },
    
    submitPromptTrees: function(tree) {
        var f = tree.data('for');
        if (f.length) { // get back to the roots (recursive calls to all waiting prompt arrows)
            var ff;
            for (var i = 0 ; i < f.length; i++) {
                toBeRecorded = f[i];
                this.submitPromptTrees(toBeRecorded);
            }
        } else { // terminal case: tree to be recorded
            this.removeFor(tree);
            this.getArrow(tree);
            this.update(tree);
        }
    },

    pairTogether: function(tail, head, immediate) {
        // TODO fix default position if head or tail not set
        var p0 = tail ? tail.position() : head.position();
        var p1 = head ? head.position() : tail.position();
        var x0 = p0.left + (tail ? tail.width() : defaultEntryWidth) / 2 + 6;
        var y0 = p0.top + (tail ? tail.height() : defaultEntryHeight);
        var x1 = p1.left + (head ? head.width() : defaultEntryWidth) / 2 - 6;
        var y1 = p1.top + (head ? head.height() : defaultEntryHeight);
        var d = $("<div class='" + (x0 < x1 ? "pairDiv" : "ipairDiv") + "'><div class='tailDiv'><div class='tailEnd'></div></div><div class='headDiv'><div class='headEnd'></div></div></div>"); // <div class='close'><a href='#'>&times;</a></div>
        this.area.append(d);
        var marge = Math.max(20, Math.min(50, Math.abs(y1 - y0)));
        d.css({
            'left': Math.min(x0, x1) + 'px',
            'top': Math.min(y0, y1) + 'px',
            'width': Math.abs(x1 - x0) + 'px',
            'height': (Math.abs(y1 - y0) + marge) + 'px'
        });
        if (this.animatePlease && !immediate) {
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
        d.on('dragstart', this.on.view.dragstart);
        d.on('dragend', this.on.view.dragend);
        d.find('.close a').click(this.on.view.close.click);

        this.addBarTo(d);
        
        // data
        d.data('tail', tail);
        d.data('head', head);
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
              n = this.putPrompt(p.left - defaultEntryWidth - 100 - c.x, p.top + d.height() / 2 - c.y);
              a = this.pairTogether(n, d);
           } else {
              n = this.putPrompt(p.left + d.width() + 100 + c.x, p.top + d.height() / 2 + c.y);
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


    moveLoadingOnArrow: function(d) {
       var p = d.position();
       $("#loading").css('top', parseInt(p.top + d.height() - defaultEntryHeight / 2) + 'px').css('left', parseInt(p.left + d.width() / 2) + 'px');
    },

    arrowEvent: function(a) {
        var self = this;
        if (a.hc === undefined ) { // a is GC-ed
            var views = a.get('views');
            views && views.forEach(function(view) { self.dismissView($(view)); });
        } else {
            var r = a.isRooted();
            var views = a.get('views');
            views && views.forEach(function(view) { $(view).find('.hook .rooted input').prop('checked', r); });
        }
    },

    
    on:{
        
        area: {
            click: function(event) {
                
                var self = $(this).data('terminal');
                var x = event.pageX;
                var y = event.pageY;
                self.putPrompt(x, y).children('input').focus();
            }
        },
        bar: {
            click: function(event) {
                var button = $(this);
                if (button.hasClass('hook')) {
                   event.stopPropagation();
                   return true;
                }
                
                var d = button.parent().parent();
                var self = d.parent().data('terminal');
                
                if (button.hasClass('close')) {
                    self.dismissView(d);
                    return false;
                }
                
                if (button.hasClass('poke')) {
                   self.update(d);
                   return false;
                }

                var a;
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
                bar.children('div').show().animate({'height': bar.height(), opacity: 1}, 50);
            },
            leave: function(event) {
                var bar = $(this);
                bar.children('div').animate({'height': '0px', opacity: 0}, 200);
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
                self.moveLoadingOnArrow(prompt);
                return false;
            },
            keypress: function(event) {
                if (event.which == 13 /* enter */) {
                    var prompt = $(this).parent();
                    var area = prompt.parent();
                    var self = area.data('terminal');
                    self.moveLoadingOnArrow(prompt);
                    event.preventDefault();
                    // record arrows that this prompt belongs too
                    self.submitPromptTrees(prompt);
                    //Arrow.commit(self.entrelacs);

                    return false;
                }
                return true;

            },
            keydown: function(event) {
                if (event.ctrlKey && event.keyCode == 13 /* Return */) {
                    var prompt = $(this).parent();
                    var area = prompt.parent();
                    var self = area.data('terminal');
                    self.moveLoadingOnArrow(prompt);

                    if ($(this).is('textarea')) {
                        // record the currently edited compound arrow (or at least this text input)
                        self.submitPromptTrees(prompt);
                        //Arrow.commit(self.entrelacs);
                    } else {
                        // turn the atom into a textarea
                        self.enlargePrompt(prompt);
                    }
                    event.preventDefault();
                    return false;
                } else if (event.ctrlKey && event.keyCode == 191 /* / */) {
                    var prompt = $(this).parent();
                    var area = prompt.parent();
                    var self = area.data('terminal');
                    event.preventDefault();
                    event.stopPropagation();
                    self.splitPrompt(prompt);
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
            split: {
                click: function(event) {
                    var prompt = $(this).parent();
                    var area = prompt.parent();
                    var self = area.data('terminal');
                    self.splitPrompt(prompt);
                    return false;
                }
            },
            fileInputButton: {
                click: function(event) {
                    $(this).parent().children('input').val('').focus();
                    event.stopPropagation();
                },
                change: function(event) {
                }
            },
        },
        view: {
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
                event.stopPropagation();
                return false;
            },
            dragstart: function(event) {
                var prompt = $(this);
                var area = prompt.parent();
                var self = area.data('terminal');
                // dragStartX = ($.browser.mozilla ? e.originalEvent.screenX : e.originalEvent.pageX);
                // dragStartY = ($.browser.mozilla ? e.originalEvent.screenY : e.originalEvent.pageY);

                self.dragStartX = event.originalEvent.screenX;
                self.dragStartY = event.originalEvent.screenY;
                event.originalEvent.dataTransfer.setData('text/plain', "arrow");
                event.originalEvent.dataTransfer.effectAllowed = "move";
                event.originalEvent.dataTransfer.dropEffect = "move";
                event.stopPropagation();
            },
            dragend: function(event) {
                var arrow = $(this);
                var area = arrow.parent();
                var self = area.data('terminal');
                if (event.originalEvent.dropEffect == "none") return false;
    
                if (self.dragOver && arrow.data('arrow')) {
                    var d = $(self.dragOver).parent();
                    self.dragOver = null;

                    self.moveLoadingOnArrow(d);

                    self.turnPromptIntoExistingView(d, $(this));
                    self.submitPromptTrees(d); // TODO Need a confirm button
                } else {
                    // dragEndX = ($.browser.mozilla ? e.originalEvent.screenX : e.originalEvent.pageX) - dragStartX
                    // dragEndY = ($.browser.mozilla ? e.originalEvent.screenY : e.originalEvent.pageY) - dragStartY
                    var dragEndX = event.originalEvent.screenX, dragEndY = event.originalEvent.screenY;

                    self.moveView(arrow, dragEndX - self.dragStartX,
                                 dragEndY - self.dragStartY, null /* no moving child */);
                }
                event.stopPropagation();
                return false;
            },
            close: {
                click: function(event) {
                    var arrow = $(this).parent().parent();
                    var area = arrow.parent();
                    var self = area.data('terminal');
                    self.dismissView(arrow);
                    return false;
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
                var b = $(this);
                var d = b.parent();
                var area = d.parent();
                var self = area.data('terminal');
                
                self.unfold(d);
                return false;
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
