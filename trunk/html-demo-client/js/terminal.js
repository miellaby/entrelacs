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
        i.on('dragenter', this.on.prompt.dragenter);
        i.on('dragleave', this.on.prompt.dragleave);
        d.on('dragstart', this.on.prompt.dragstart);
        d.on('dragend', this.on.prompt.dragend);
        d.find('.close a').click(this.on.tree.close.click);
        this.addBarTo(d);
        
        // data
        d.data('for', []);
        d.data('children', []);
        
        // file uploading
        d.append("<span class='fileinput-button'><span>...</span><input type='file' name='files[]'></span>");
        d.children('.fileinput-button').click(this.on.prompt.fileInputButton.click).change(this.on.prompt.fileInputButton.change);
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
    
    closePrompt: function(prompt) {
        this.dismissArrow(prompt);
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

    update: function(d) {
        var arrow = d.data('arrow');
        var self = this;
        d.css('marginTop','-5px').animate({marginTop: 0});
        var promise = self.entrelacs.isRooted(arrow);
        promise.done(function () {
            d.find('.hook .rooted input').prop('checked', arrow.isRooted());
            if (d.hasClass('known')) { // know
                d.removeClass('known');
                d.addClass('kept');
                d.find('.hook .rooted input').prop('disabled', false);
            }
            var promise = self.entrelacs.getChildren(arrow);
            promise.done(function() {
                // TODO load arrows
            });
        });
    },
    
    getArrow: function(d) {
        // THAT'S WHEN IT'S COME INTERESTING
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
        this.update(d);
        return arrow;
    },
    
    submitArrows: function(tree) {
        var f = tree.data('for');
        if (f.length) { // get back to the roots (recursive calls to all waiting prompt trees)
            var ff;
            for (var i = 0 ; i < f.length; i++) {
                toBeRecorded = f[i];
                this.submitArrows(toBeRecorded);
            }
        } else { // terminal case: arrow to be recorded
            this.getArrow(tree);
            
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
        d.on('dragstart', this.on.tree.dragstart);
        d.on('dragend', this.on.tree.dragend);
        d.find('.close a').click(this.on.tree.close.click);

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
                    } else {
                        // turn the atom into a textarea
                        self.enlargePrompt(prompt);
                    }
                    e.preventDefault();
                    return false;
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
        tree: {
            dragenter: function(event) {
            },
            dragleave: function(event) {
            },
            dragstart: function(event) {
            },
            dragend: function(event) {
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
