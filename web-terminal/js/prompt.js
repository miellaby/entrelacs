function Prompt(string, terminal, x, y, immediate) {
    var self = this;    

    var d = $("<div class='prompt'><input type='text'></input></div>");
    d.css({ 'left': x + 'px', 'top': y + 'px' });
    View.call(this, null, terminal, d);
    var w = d.width();

    if (terminal.animatePlease && !immediate) {
        d.css({
            'opacity': 0.1,
            'width': '10px',
            'margin-left': 0,
            'margin-top': -d.height() + 'px'
        }).animate({
            'opacity': 1,
            'margin-left': -parseInt(w / 2) + "px",
            'width': w
        }, 200, 'swing', function() { self.d.css('width', 'auto');});
    } else {
        d.css({
            'margin-left': -parseInt(w / 2) + 'px',
            'margin-top': -d.height() + 'px'
        });
        //self.d.css({'left': x - parseInt(terminal.defaultEntryWidth / 2) + 'px', 'y': y - d.height() + 'px'});
    }
    
    $.extend(this.on, {
        focus: function(event) {
            self.d.children('.fileinput-button,.split,.wiki-checkbox,.ok-button').animate({'margin-right': '0px', opacity: 1}, 200).fadeIn();
            self.bar.children('div,a').show().animate({'height': self.bar.height(), opacity: 1}, 200);
            return true;
        },
        
        blur: function(event) {
            self.closeIfLoose();
            var flies = self.d.children('.fileinput-button,.split,.wiki-checkbox,.ok-button');
            flies.delay(300).animate({'margin-right': '40px', opacity: 0}, 200).fadeOut();
            if (!self.bar.hasClass('hover'))
                self.bar.children('div,a').animate({'height': '0px', opacity: 0}, 500);
            return true;
        },
        
        click: function(e) {
            return false;
        },

        keypress: function(event) {
            if (event.which == 13 /* enter */) {
                self.terminal.moveLoadindBarOnView(self);
                // record arrows that this prompt belongs too
                self.confirmDescendants();
                // self.terminal.commit();
                // event.preventDefault();
                return false;
            }
            return true;

        },

        keydown: function(event) {
            if (event.ctrlKey && event.keyCode == 13 /* Return */) {
                self.terminal.moveLoadindBarOnView(self);

                if (self.isEnlarged()) {
                    self.terminal.moveLoadindBarOnView(self);
                    // record arrows that this prompt belongs too
                    self.confirmDescendants();
                } else {
                    self.enlarge();
                }
                return false;
                
            } else if (event.ctrlKey && (event.keyCode == 191 /* / */ || event.keyCode == 80 /* p */)) {
                self.split();
                return false;
                
            } else if (event.keyCode == 27 /* escape */) {
                self.close();
                return false;

            } else if (event.keyCode == 9 /* tab */) {
                var f = self.for;
                for (var i = 0; i < f.length; i++) { // search into loose children
                    var next = event.shiftKey ? self.terminal.findPrevious(f[i], self) : self.terminal.findNext(f[i], self);
                    if (next) {
                        next.focus();
                        return false;
                    }
                }

                if (f.length) {
                    var ff;
                    while ((ff = f[f.length - 1].for) && ff.length) {
                        f = ff ;
                    }
                    var a = self.terminal.leave(f[f.length - 1], event.shiftKey /* in/out */);
                } else {
                    var a = self.terminal.leave(self, event.shiftKey /* out */);
                }
                return false;
            }

            return true;

        },

        split: {
            click: function(event) {
                self.split();
                return false;
            }
        },

        fileInputButton: {
            click: function(event) {
                event.stopPropagation();
            },
        },
        wikiButton: {
            click: function(event) {
                self.setWikiFormated();
                self.focus();
                return false;
            },
        },        
        okButton: {
            click: function(event) {
                self.terminal.moveLoadindBarOnView(self);
                // record arrows that this prompt belongs too
                self.confirmDescendants();
                return false;
            }
        },
        dragenter: function(event) {
            
            if ($(this).is("input[type='text'],textarea")) {
                event.originalEvent.dataTransfer.dropEffect = "move";
                self.terminal.dragOver = self;
                $(this).stop(true, true).animate({'border-bottom-width': '3px'}, 100);
            }
            //event.preventDefault();
            event.stopPropagation();
            //return false;
        },
        
        dragleave: function(event) {
            if ($(this).is("input[type='text'],textarea")) {
                // Check the mouseEvent coordinates are outside of the element rectangle
                var rect = this.getBoundingClientRect();
                event.clientX = event.originalEvent.clientX;
                event.clientY = event.originalEvent.clientY ;
                // console.log('' + rect.width + 'x' + rect.height + '@' + rect.left + ',' + rect.top + ' ' + event.pageX +  ' ' + event.pageY);
                if (event.clientX >= rect.left + rect.width  || event.clientX <= rect.left
                    || event.clientY >= rect.top + rect.height || event.clientY <= rect.top) {
                    $(this).stop(true, true).animate({'border-bottom-width': '1px'}, 100);
                    self.terminal.dragOver = null;
                }
            }
            //event.preventDefault();
            event.stopPropagation();
            //return false;
        }
    });
    
    var i = d.children('input[type="text"]');
    // set listeners
    i.blur(this.on.blur);
    i.click(this.on.click);
    i.focus(this.on.focus);
    i.keypress(this.on.keypress);
    i.keydown(this.on.keydown);
    i.on('dragenter', this.on.dragenter);
    i.on('dragleave', this.on.dragleave);
    i.on('dragover', function() { return false; });
    
    // split button
    $("<a class='split'>&nbsp;|&nbsp;</a>")
            .prependTo(d)
            .click(this.on.split.click);

    // file upload button
    $("<span class='fileinput-button'><span>...</span><input type='file' name='files[]'></span>")
            .appendTo(d)
            .click(this.on.fileInputButton.click);

    // wiki button
    var wb = $("<a class='wiki-checkbox'>.__.</a>")
        .appendTo(d)
        .click(this.on.wikiButton.click);

    // Cross-domain form posts can't be queried back!
    // So we build up a secret and pair it with our uploaded file, then we get back our secret child!
    var secret = Math.random().toString().substring(2) + Date.now();
        
    // One doesn't wan the blob to be evaluated ; so here is a prog
    var prog = '/macro/x/arrow//escape+escape/' + secret + '/var+x';
    
    d.find('input[type="file"]').ajaxfileupload({
        'validate_extensions': false,
        'action': terminal.entrelacs.serverUrl + prog,
        'onStart':  function() {
            self.terminal.uploadCount++;
            self.terminal.moveLoadindBarOnView(self);
            self.terminal.loading.show();
        },
        'onComplete': function(response) {
            if (typeof response == "Object" && response.status === false)
                return;
                
            self.terminal.uploadCount--;
            // response is not readable
            // however one can search for the arrow child
            var query = self.terminal.entrelacs.getChildren(secret);
            query.done(function(response) {
                self.turnIntoBlobView(response.getTail().getHead() /* head of first outgoing arrow */);
            });
        },
        'onCancel': function() {
            console.log('cancelling: '); console.log(this);
            self.closeIfLoose();
        },
    });
    
    if (string) {
        var preformated = (~string.indexOf("\n") || ~string.indexOf("\t"));
        if (preformated) {
            this.enlarge();
            d.children('textarea').text(string);
        } else {
            i.val(string);
        }
    }
   
};

$.extend(Prompt.prototype, View.prototype, {
    enlarge: function() {
        var d = this.d;
        var w0 = d.width();
        var i = d.children('input');
        var v = i.val();
        d.children('.fileinput-button,.split').detach();
        var textPosition = i[0].selectionStart;
        i.detach();
        i = $("<textarea></textarea>").text(v).appendTo(d);
        i[0].setSelectionRange(textPosition, textPosition);
        i.blur(this.on.blur);
        i.focus(this.on.focus);
        i.keydown(this.on.keydown);
        i.click(this.on.click);
        i.on('dragenter', this.on.dragenter);
        i.on('dragleave', this.on.dragleave);
        i.focus();
        
        // ok button
        var ok = $("<a class='ok-button'>OK</a>")
            .show()
            .appendTo(d)
            .click(this.on.okButton.click);

        var w = d.width();
        d.css({
            'margin-left': -parseInt(w / 2) + 'px',
            'margin-top': -d.height() + 'px'
        });
    },

    isEnlarged: function() {
        return this.d.children('textarea').length > 0;
    },
            
    setWikiFormated: function(isWikiFormated) {
        if (isWikiFormated === undefined) {
            isWikiFormated = !this.isWikiFormated();
        }
        
        if (isWikiFormated && !this.isEnlarged())
           this.enlarge();

        var c = this.d.children('.wiki-checkbox');
        c.text(isWikiFormated ? 'Wiki' : 'raw');
    },
                    
    isWikiFormated: function() {
        return this.d.children('.wiki-checkbox').text() === 'Wiki';
    },
            
    turnIntoAtomView: function() {
        var string = this.d.children('input[type="text"],textarea').val();
        var arrow = Arrow.atom(string);
        var p = this.d.position();
        var atomView;
        if (this.replaced) {
            console.log('prompt on replaced');
        }

        if (this.isWikiFormated()) {
            arrow = Arrow.pair(Arrow.atom('Content-Typed'), Arrow.pair(Arrow.atom('text/x-creole'), arrow));
            atomView = new BlobView(arrow, this.terminal, p.left, p.top);
        } else {
            atomView = new AtomView(arrow, this.terminal, p.left, p.top);
        }
        this.replaceWith(atomView);
        return atomView;
    },

        
    turnIntoBlobView: function(blob) {
        var p = this.d.position();
        var blobView = new BlobView(blob, this.terminal, p.left, p.top);
        this.replaceWith(blobView);
        return blobView;
    },
    
    closeIfLoose: function() {
        if (this.isLoose()) {
            var self = this;
            setTimeout(function() {
                if (document.hasFocus()
                        && !self.d.find('input[type="text"],textarea').is(':focus')
                        && self.isLoose())
                    self.close();
                // TODO : if the document has lost focus, one should check loose prompts latter
            }, 1000);
        }
    },

    split: function() {
        var i = this.d.children('input[type="text"],textarea');
        var v = i.val();
        var textPosition = i[0].selectionStart;

        var p = this.d.position();

        // peek a counting circle point
        var c = this.terminal.getPointOnCircle(50);
        
        // build a tail prompt
        var t = new Prompt(v, this.terminal, p.left - 100 - c.x, p.top - 100 - c.y);

        // build a head prompt
        var h = new Prompt("", this.terminal, p.left + 100 + c.x, p.top - 50 + c.y);

        // pair head and tail as loose arrows
        var pairView = new PairView(null, this.terminal, t, h);

        // replace prompt with pair
        this.replaceWith(pairView);

        // focus on tail prompt
        t.d.children('input[type="text"]').focus()[0].setSelectionRange(textPosition, textPosition);

        return pairView;
    },

    /** focus */
    focus: function () {
        this.d.children('input[type="text"],textarea').focus();
        return true;
    },

    confirm: function() {
        if (this.arrow) return this;
        return this.turnIntoAtomView();
    },

    isPairView: function() { return false; }
});
