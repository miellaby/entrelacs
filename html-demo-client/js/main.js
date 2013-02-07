var server = 'http://miellaby.selfip.net:8008/';
var area = null;
var animatePlease = true;
var defaultEntryWidth;
var defaultEntryHeight;
var dragStartX, dragStartY;
var dragOver = null;

function isFor(d, a) {
    var list = d.data('for');
    for (var i = 0; i < list.length; i++) {
       var c = list[i];
       if  (c[0] === a[0]) return true;
    }
    return false;
}

function updateFor(d, a, newA) {
    var list = d.data('for');
    for (var i = 0; i < list.length; i++) {
       var c = list[i];
       if  (c[0] === a[0]) {
           list.splice(i, 1, newA);
           break;
       }
    }
    return list;
}

function removeFor(d, a) {
    var list = d.data('for');
    for (var i = 0; i < list.length; i++) {
       var c = list[i];
       if  (c[0] === a[0]) {
           list.splice(i, 1);
           break;
       }
    }
    return list;
}

function isLoose(d) {
    return (!d.data('for').length && !d.data('children').length);
}

function hasChild(d, a) {
    var list = d.data('children');
    for (var i = 0; i < list.length; i++) {
       var c = list[i];
       if  (c[0] === a[0]) return true;
    }
    return false;
}

function updateChild(d, a, newA) {
    var list = d.data('children');
    for (var i = 0; i < list.length; i++) {
       var c = list[i];
       if  (c[0] === a[0]) {
           list.splice(i, 1, newA);
           break;
       }
    }
    return list;
}

function removeChild(d, a) {
    var list = d.data('children');
    for (var i = 0; i < list.length; i++) {
       var c = list[i];
       if  (c[0] === a[0]) {
           list.splice(i, 1);
           break;
       }
    }
    return list;
}

function dismissArrow(d, direction /* false = down true = up */) {
   // guard double detach
   if (d.data('detached')) return;
   // for guard
   d.data('detached', true);

   if (d.hasClass('pairDiv') || d.hasClass('ipairDiv')) {
        // propagate deletion to tail
        var t = d.data('tail');
        if (t) {
           removeFor(t, d);
           removeChild(t, d);
           if (isLoose(t) && !t.data('lastPosition'))
               dismissArrow(t, true);
        }

        // propagate deletion to head
        var h = d.data('head');
        if (h) {
           removeFor(h, d);
           removeChild(h, d);
           if (isLoose(h) && !h.data('lastPosition'))
               dismissArrow(h, true);
        }

   }

   // propagate to loose child
   if (!direction) { //  when going "down"
       var f = d.data('for'); 
       while (f.length) {
            dismissArrow(f[0]);
       }
   }

   // fold children
   var children = d.data('children');
   for (var i = 0; i < children.length; i++) {
        var c = children[i];
        // var subchildren = c.data('children');
        // TODO show unfold buttons
        if (c.data('head') && c.data('head')[0] === d[0]) {
            c.children('.headDiv').height(0).width(20).css('border-style', 'dashed');
            u = $("<button class='unfoldHead'>+</button>");
            u.click(unfoldClick);
            u.appendTo(c);
            c.removeData('head');
            continue;
        }
        if (c.data('tail') && c.data('tail')[0] === d[0]) {
            c.children('.tailDiv').height(0).width(20).css('border-style', 'dashed');
            u = $("<button class='unfoldTail'>+</button>");
            u.click(unfoldClick);
            u.appendTo(c);
            c.removeData('tail');
            continue;
        }
    }
    var lastPosition = d.data('lastPosition');
    if (lastPosition) {
       var instruction = server + '/unlinkTailAndHead/escape//position+' + d.data('url') + lastPosition;
       $.ajax({url: instruction, xhrFields: { withCredentials: true }});
    }

    d.detach();
}

// find an editable atom into a tree
function findNextInto(d) {
    if (d.hasClass('atomDiv')) { // atom
        return d.data('for').length ? d /* found */: null;
    } else { // pair
       // check in tail then in head
       var n = d.data('tail') ? findNextInto(d.data('tail')) : null;
       if (!n && d.data('head'))
            n = findNextInto(d.data('head'));
       return n;
    }
}

// get an arrow uri
function buildUri(d) {
    var uri = d.data('url');
    if (uri) {
      // reuse known url if any
      return uri
    } else if (d.hasClass('atomDiv')) { // atom
        var v;
        if (d.hasClass('kept'))
            v = d.children('span').text();
        else {
            v = d.children('input').val();
        }
        return encodeURIComponent(v);
       
    } else { // pair ends URI
       var tailUri = buildUri(d.data('tail'));
       var headUri = buildUri(d.data('head'));
       return '/' + tailUri + '+' + headUri;
    }
}

function turnInputIntoText(d) {
    var i = d.children('input');
    if (!i.length) return;
    var v = i.val() || 'EVE';
    
    var w0 = i.width();
    d.addClass('kept');
    d.children('.close').show();
    i.detach();
    var s = $("<span></span>").text(v).appendTo(d);
    s.click(onAtomClick);
    var w = s.width();
    s.css({width: (w < 100 ? 100 : w) + 'px', 'text-align': 'center'});
    d.css('left', (w < w0 ? '+=' + ((w0 - w) / 4) : '-=' + ((w - w0) / 4)) + 'px');
    d.children('.fileinput-button').detach();
}

function unlooseArrow(d, c) {
    // already connected arrow?
    if (c && !isFor(d, c)) return;

    if (d.hasClass('atomDiv')) { // atom
       // make it immutable
       turnInputIntoText(d);
    } else if (d.hasClass('pairDiv') || d.hasClass('ipairDiv')) { // pair
       // unloose its end
       unlooseArrow(d.data('tail'), d);
       unlooseArrow(d.data('head'), d);
       d.addClass('kept');
    }

    removeFor(d, c);
    if (c && !hasChild(d, c)) {
       var children = d.data('children');
       children.push(c);
    }
}

function assimilateArrow(d) {
    var uri = buildUri(d);
    // assimilation request
    var request = $.ajax({url: server + 'escape/' /* please not that one ever escapes submited arrow */ + uri + '?iDepth=0', xhrFields: { withCredentials: true }});
    var id;
    d.data('url','pending...');
    
    request.done(function(data, textStatus, jqXHR) {
       console.log(data);
       // save url
       d.data('url', data);
       // toast url
       toast(d, data);
       // save position
       var p = d.position();
       var instruction = server + 'linkTailWithHead/escape//position+' + d.data('url') + '/' + parseInt(p.left) + '+' + parseInt(p.top);
       $.ajax({url: instruction, xhrFields: { withCredentials: true }}).done(function(lastPosition) {
            d.data('lastPosition', lastPosition);
            findPositions(d);
       });
       
    });
}

function recordArrow(d) {
    // System upload
    assimilateArrow(d);
    
    // unloose arrow
    unlooseArrow(d);
}

// search for the next editable atom
function findNext(d, from, leftOnly) {
   var f = d.data('for');
   if ((d.hasClass('pairDiv') || d.hasClass('ipairDiv')) && d.data('head') && d.data('head')[0] !== from[0]) { // search into head
      var n = findNextInto(d.data('head'));
      if (n) return n;
   }
   
   for (var i = 0; i < f.length; i++) { // search into loose children
        var n = findNext(f[i], d, leftOnly);
        if (n) return n;
   }
   
  if (!leftOnly && d.hasClass('pairDiv') && d.data('tail') && d.data('tail')[0] !== from[0]) { // search into tail
      var n = findNextInto(d.data('tail'));
      if (n) return n;
   }

   // search fail: no editable
   return null;
}

function onEntryKeypress(e) {
    var next = null;
    console.log(e);
    
    if (e.which == 47 /* / */ && !$(this).data('escape')) {
        var d = $(this).parent();
        e.preventDefault();
        turnEntryIntoPair(d);
        return false;
    } else if (e.which == 43 /* + */ && !$(this).data('escape')) {
        var d = $(this).parent();
        e.preventDefault();
        var f = d.data('for');
        for (var i = 0; i < f.length; i++) { // search into loose children
           var next = findNext(f[i], d, true /* leftOnly */);
           if (next) {
             next.children('input').focus();
             return;
           }
        }
        if (!f.length) {
            var a = leave(d, false /* out */);
            d.data('for').push(a);
            return false;
        }
        
        // overloading + ==> open a pair with the whole loose child arrow
        var ff;
        while ((ff = f[f.length - 1].data('for')) && ff.length) {
           f = ff ;
        }
        var a = leave(f[f.length - 1], false /* out */);
        f[f.length - 1].data('for').push(a);
        return false;
        
     } else if (e.which == 13 /* return */) {
        var d = $(this).parent();
        e.preventDefault();
        // record the whole child arrow
        var f = d.data('for');
        if (f.length) {
           var ff;
           while ((ff = f[f.length - 1].data('for')) && ff.length) {
              f = ff ;
           }
           recordArrow(f[f.length - 1]);
        } else {
           recordArrow(d);
        }
        return false;
    }
    return true;
}

function onEntryFocus(e) {
    var i = $(this);
    var d = i.parent();
    d.removeData('preserve')
    return true;
}

function onAtomClick(e) {
    var d = $(this).parent();
    if (!d.hasClass('kept')) // filter out unfinished atom
        return false;
    d.css('marginTop','-10px').animate({marginTop: 0});
    findPositions(d);
    return false;
}

var circleAngle = 0;
function getPointOnCircle(radius) {
   circleAngle += 4;
   var x = radius * Math.sin(2 * Math.PI * circleAngle / 23);
   var y = radius * Math.cos(2 * Math.PI * circleAngle / 23);
   var p = { x: parseInt(x), y: parseInt(y)};
   console.log(p);
   return p;
}

function leave(d, inOut) {
   var p = d.position();
   var c = getPointOnCircle(100);
   var n, a;
   if (inOut) {
      n = addAtom(p.left - defaultEntryWidth - 100 - c.x, p.top + d.height() / 2 - c.y);
      a = addPair(n, d, animatePlease);
   } else {
      n = addAtom(p.left + d.width() + 100 + c.x, p.top + d.height() / 2 + c.y);
      a = addPair(d, n, animatePlease);
   }
   n.data('for').push(a);
   n.children("input").focus();
   return a;
}

// compute arrow definition as a headOf/tailOf sequence
function getDefinition(d) {
    var def = '';
    if (d.data('url')) { // reuse url
        def = '/escape+' + d.data('url');
    } else {
        var c = d;
        var cc = c.data('children')[0];
        while (cc) {
            def += (cc.data('head') && cc.data('head')[0] === c[0] || cc.data('tail') && cc.data('tail')[0] !== c[0]) ? '/headOf+' : '/tailOf+';
            if (cc.data('url')) break;
            c = cc;
            cc = c.data('children')[0];
        }
        if (!cc) {
           throw 'undef';
        }
        def += '/escape+' + cc.data('url');
    }
    return def;
}

function unfold(d, unfoldTail) {
    // restore end
    d.children(unfoldTail ? '.tailDiv' : '.headDiv').height('100%').width('').css('border-style', '');
    d.children(unfoldTail ? '.unfoldTail' : '.unfoldHead').detach();

    // compute unfolded arrow definition as a headOf/tailOf sequence
    var def = (unfoldTail ? '/tailOf+' : '/headOf+') + getDefinition(d);
       
    // request
    var request = $.ajax({url: server + def + '?iDepth=1', xhrFields: { withCredentials: true }});
    var id;
    
    request.done(function(data, textStatus, jqXHR) {
        console.log(data);
        var p = d.position();
        if (data[0] == '/') { // parent is a pair
            if (unfoldTail) {
                a = addFoldedPair(p.left, p.top);
                d.data('tail', a);
            } else {
                a = addFoldedPair(p.left + d.width(), p.top);
                d.data('head', a);
            }
            // save url
           a.data('url', data);
           // toast url
           toast(a, data);
        } else { // parent is an atom: TODO fix API can't detect /$ string
            if (unfoldTail) {
                a = addAtom(p.left - defaultEntryWidth / 2 - 3, p.top - defaultEntryHeight, data);
                d.data('tail', a);
            } else {
                a = addAtom(p.left + d.width() - defaultEntryWidth / 2 + 3, p.top - defaultEntryHeight, data);
                d.data('head', a);
            }
            turnInputIntoText(a);
        }
        // connectivity
        a.data('children').push(d);
    });
}

function unfoldClick(e) {
   var i = $(this);
   var d = i.parent();
   unfold(d, i.hasClass('unfoldTail'));
   return false;
}

function onHookClick(e) {
   var i = $(this);
   var d = i.parent().parent();
   var a;
   if (i.hasClass('poke')) {
       d.css('marginTop','-10px').animate({marginTop: 0});
       if (!d.hasClass('kept')) return false; // filter out unfinished atom
       findPositions(d);
       return false;
   }
   
   if (i.hasClass('in'))
      a = leave(d, true);
   else if (i.hasClass('out'))
      a = leave(d, false);
   if (a)
       d.data('for').push(a);
   return false;
}

function onCloseClick(e) {
    var d = $(this).parent();
    dismissArrow(d);
    return false;
}

var toaster, toasterHeight;
function toast(source, text) {
    var p = source.position();
    var w = source.width();
    var topStart = ((source.hasClass('pairDiv') || source.hasClass('ipairDiv')) ? p.top + source.height() : p.top) - toasterHeight;
    toaster.animate({ left: p.left + w / 2, top: topStart}, 50, 'linear', function() { toaster.show().text(text).css({left: '-=' + (toaster.width()/2)+'px', opacity: 0.5}).animate({ top: "-=50px", opacity: 0}, 2000);});
    
}

function recordOrDetachAtom(d) {
    if (isLoose(d)) {
        setTimeout(function() {
            if (!d.children('input').is(':focus') && isLoose(d) && !d.data('url') && !d.data('preserve'))
                 dismissArrow(d);
        }, 1000);
    }
}

function onEntryBlur(event) {
    var d = $(this).parent();
    recordOrDetachAtom(d);
}

function onEntryKeydown(e) {
    console.log(e);
    if (e.keyCode == 27) {
        e.preventDefault();
        dismissArrow($(this).parent());
    } else if (e.keyCode == 9) {
        var d = $(this).parent();
        e.preventDefault();
        var f = d.data('for');
        for (var i = 0; i < f.length; i++) { // search into loose children
           var next = findNext(f[i], d);
           if (next) {
             next.children('input').focus();
             return;
           }
        }
        if (f.length) {
           var ff;
           while ((ff = f[f.length - 1].data('for')) && ff.length) {
              f = ff ;
           }
           var a = leave(f[f.length - 1], false /* out */);
           f.data('for').push(a);
        } else {
           var a = leave(d, false /* out */);
           d.data('for').push(a);
        }
    }
}

function updateRefs(d, oldOne, newOne) {
   updateFor(d, oldOne, newOne);
   updateChild(d, oldOne, newOne);
}


function updateDescendants(a, newA) {
   var f = a.data('for');
   for (var i = 0; i < f.length; i++) { // search into loose children
      rewirePair(f[i], a, newA);
   }
   var children = a.data('children');
   for (var i = 0; i < children.length; i++) {
      rewirePair(children[i], a, newA)
   }
}

function rewirePair(a, oldOne, newOne) {
   var oldTail = a.data('tail');
   var oldHead = a.data('head');

   var tailChanged = (oldTail && oldTail[0] === oldOne[0]);
   var headChanged = (oldHead && oldHead[0] === oldOne[0]);

   var newTail = tailChanged ? newOne : oldTail;
   var newHead = headChanged ? newOne : oldHead;
   var newA = addPair(newTail, newHead);
   
   // class copy
   if (a.hasClass('kept'))
      newA.addClass('kept');
   // data copy
   var oldData = a.data();
   for (var dataKey in oldData) {
      if (dataKey == 'head' || dataKey == 'tail') continue;
      newA.data(dataKey, oldData[dataKey]);
   }
   
   var f = a.data('for');
   newA.data('for', f);
   var children = a.data('children');
   newA.data('children', children);
   var url = a.data('url');
   if (url)
      newA.data('url', url);
   
   // back refs updating
   if (newTail)
       updateRefs(newTail, a, newA);
       
   if (newHead && !(newTail && newTail[0] === newHead[0])) {
      updateRefs(newHead, a, newA);
   }

   updateDescendants(a, newA);
   
   a.detach();

   return newA;
}

function turnEntryIntoExistingArrow(d, a) {
    if (d.data('children').length) {
       throw "connected arrows are immutable";
    }
    
    var p = d.position();
    var w = d.width();
    var f = d.data('for');

    // merge for list
    a.data('for', a.data('for').concat(d.data('for')));
     
    // if entry belonged to a loose arrow, one rewires to a
    for (var i = 0; i < f.length; i++) { // search into loose children
       rewirePair(f[i], d, a);
    }
    
    // detach entry-atom
    d.detach();
}


function turnEntryIntoPair(d) {
    if (d.data('children').length) {
       throw "connected arrows are immutable";
    }
    
    var v = d.children('input').val();
    var p = d.position();
    var w = d.width();
    var f = d.data('for');
    // peek a counting circle point
    var c = getPointOnCircle(50);
    
    // build a tail entry
    var t = addAtom(p.left - d.width() / 2 - 50 - c.x, p.top + d.height() / 2 - 100 - c.y, v);

    // build a head entry
    var h = addAtom(p.left + d.width() / 2 + 50 + c.x, p.top + d.height() / 2 - 100 - c.y, '');

    // pair head and tail as loose arrows
    var a = addPair(t, h, animatePlease);
    t.data('for').push(a);
    h.data('for').push(a);

    // copy data from converted atom to obtained pair
    a.data('for', f);
     
    // if atom belonged to a loose arrow, one rewires to the obtained pair
    for (var i = 0; i < f.length; i++) { // search into loose children
       rewirePair(f[i], d, a);
    }
    
    // if atom had content, it went to the pair tail (see addAtom call)
    if (v !== '') {
       // and one builds a second pair
       turnEntryIntoPair(h);
    } else {
       t.children('input').focus();
    }

    // detach atom
    d.detach();
}

function showMovingGhost(x, y, d) {
     var p = d.position();
     var nx = p.left + (d.width() - toaster.width()) / 2;
     var ny = p.top + d.height();
     var ghost = $("<div class='ghost'>" + d.data('url') + '</div>').appendTo(area).css({left: (x  + (d.width() - toaster.width()) / 2) + 'px', top: (y + d.height()) + 'px'});
     ghost.animate({left: nx, top: ny, opacity: 0.5}, function() { ghost.detach(); });
}

function findPositions(d) {
    if (!d.data('url')) {
        var def = getDefinition(d);
        var request = $.ajax({url: server + def + '?iDepth=0', xhrFields: { withCredentials: true }});
        request.done(function(data, textStatus, jqXHR) {
           d.data('url', data);
           findPositions(d);
        });
    } else {
        var request = $.ajax({url: server + 'partnersOf/escape/position+' + d.data('url') + '?iDepth=10', xhrFields: { withCredentials: true }});
        var lp = d.data('lastPosition');
        request.done(function(data, textStatus, jqXHR) {
            console.log(data);
            var p = data.match(/\/[0-9]*\+[0-9]*/g) || [];
            for (var i = 0; i < p.length; i++) {
                if (p[i] == lp) continue;
                var xy = p[i].match(/[0-9]+/g);
                showMovingGhost(parseInt(xy[0]), parseInt(xy[1]), d);
            }
        });
        
    }
    
    
}

function moveArrow(d, offsetX, offsetY, movingChild) {
    d.css({ 'opacity': 1,
            'left': (d.position().left  + offsetX) + 'px',
            'top': (d.position().top + offsetY) +'px'
    });
    if (d.hasClass('pairDiv') ||d.hasClass('ipairDiv')) {
       if (d.data('head')) {
           moveArrow(d.data('head'), offsetX, offsetY, d);
       }
       if (d.data('tail') && !(d.data('head') && d.data('tail')[0] === d.data('head')[0])) {
           moveArrow(d.data('tail'), offsetX, offsetY, d);
       }
    }
    
    updateDescendants(d, d, movingChild);
    
    var lastPosition = d.data('lastPosition');
    if (lastPosition) {
       var p = d.position();
       var instruction = server + '/unlinkTailAndHead/escape//position+' + d.data('url') + lastPosition
                              + '/,/linkTailWithHead/escape//position+' + d.data('url') + '/' + parseInt(p.left) + '+' + parseInt(p.top);
       $.ajax({url: instruction, xhrFields: { withCredentials: true }}).done(function(newPosition) {
            d.data('lastPosition', newPosition);
       });
    }
}

function onDragStart(e) {
    dragStartX = e.originalEvent.pageX;
    dragStartY = e.originalEvent.pageY;
    return true;
}

function onDragEnd(e) {
    var d = $(this);
    console.log(e);
    if (dragOver && $(this).hasClass('kept')) {
        var d = $(dragOver).parent();
        turnEntryIntoExistingArrow(d, $(this));
    } else {
        moveArrow(d, e.originalEvent.pageX - dragStartX, e.originalEvent.pageY - dragStartY, null /* no moving child */);
    }
    return true;
}

function addFoldedPair(x, y) {
    var d = $("<div class='pairDiv'><div class='tailDiv'><div class='tailEnd'></div></div><div class='headDiv'></div><div class='hook'><div class='in'>&uarr;</div> <div class='poke'>o</div> <div class='out'>&darr;</div></div><button class='unfoldTail'>+</button><button class='unfoldHead'>+</button><div class='close'>&times;</div></div>");
    area.append(d);
    d.css({left: (x - 75) + 'px',
           top: (y - 45) + 'px',
           height: '45px',
           width: '150px'});
    d.children('.headDiv,.tailDiv').height(0).width(20).css('border-style', 'dashed');

    // set listeners
    d.children('.hook').hover(function(){$(this).children('.in,.out,.poke').fadeIn(100);}, function() {$(this).children('.in,.out,.poke').fadeOut(500);});
    d.children('.hook').children('.in,.out,.poke').click(onHookClick);
    d.attr('draggable', true);
    d.on('dragstart', onDragStart);
    d.on('dragend', onDragEnd);
    d.children('.close').click(onCloseClick);
    d.children('.unfoldTail,.unfoldHead').click(unfoldClick);

    // data
    d.data('for', []);
    d.data('children', []);
    return d;
}

function addPair(tail, head, animate) {
    var p0 = tail ? tail.position() : (head ? head.position() : {left: area.width() / 2, top: area.height() / 2});
    var p1 = head ? head.position() : (tail ? tail.position() : {left: area.width() / 2, top: area.height() / 2});
    var x0 = p0.left + (tail ? tail.width() : defaultEntryWidth) / 2 + 3;
    var y0 = p0.top + (tail ? tail.height() : defaultEntryHeight);
    var x1 = p1.left + (head ? head.width() : defaultEntryWidth) / 2 - 3;
    var y1 = p1.top + (head ? head.height() : defaultEntryHeight);
    var d = $("<div class='" + (x0 < x1 ? "pairDiv" : "ipairDiv") + "'><div class='tailDiv'><div class='tailEnd'></div></div><div class='headDiv'></div><div class='hook'><div class='in'>&uarr;</div> <div class='poke'>o</div> <div class='out'>&darr;</div></div><div class='close'>&times;</div></div>");
    area.append(d);
    var marge = Math.max(20, Math.min(50, Math.abs(y1 - y0)));
    d.css({
        'left': Math.min(x0, x1) + 'px',
        'top': Math.min(y0, y1) + 'px',
        'width': Math.abs(x1 - x0) + 'px',
        'height': (Math.abs(y1 - y0) + marge) + 'px'
    });
    if (animate) {
        d.hide();
        d.children('.tailDiv').animate({'height': (marge + ((y0 < y1) ? y1 - y0 : 0)) + 'px'});
        d.children('.headDiv').animate({'height': (marge + ((y1 < y0) ? y0 - y1 : 0)) + 'px'});
        d.fadeIn(500);
    } else {
        d.children('.tailDiv').css({'height': (marge + ((y0 < y1) ? y1 - y0 : 0)) + 'px'});
        d.children('.headDiv').css({'height': (marge + ((y1 < y0) ? y0 - y1 : 0)) + 'px'});
    }
    
    // set listeners
    d.children('.hook').hover(function(){$(this).children('.in,.out,.poke').fadeIn(100);}, function() {$(this).children('.in,.out,.poke').fadeOut(500);});
    d.children('.hook').children('.in,.out,.poke').click(onHookClick);
    d.attr('draggable', true);
    d.on('dragstart', onDragStart);
    d.on('dragend', onDragEnd);
    d.children('.close').click(onCloseClick);

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
}

function onDragEnterEntry(e) {
    dragOver = this;
    console.log(e);

    $(this).animate({'border-width': 6, left: "-=3px"}, 100);
}

function onDragLeaveEntry(e) {
    console.log(e);
    $(this).animate({'border-width': 2, left: "+=3px"}, 100);
    setTimeout(function() { dragOver = null; }, 0); // my gosh
}

function onFileInputClick(e) {
    e.stopPropagation();
    var d = $(this).parent();
    setTimeout(function() { // terrible trick
        d.children('input').focus();
        d.data('preserve', true);
    }, 0); // my gosh
}

function onFileInputChange(e) {
    $(this).appendTo('#upload_form');
    $('#upload_form').submit();
}

function addAtom(x, y, initialValue) {
    var d = $("<div class='atomDiv'><input type='text'></input><div class='close'>&times;</div><div class='hook'><div class='in'>&uarr;</div> <div class='poke'>o</div>  <div class='out'>&darr;</div></div></div>");
    area.append(d);
    if (animatePlease) {
        d.css('opacity', 0.1).animate({ opacity: 1});
    }
    d.css({ 'left': x + 'px',
            'top': y + 'px' });
    
    var i = d.children('input');
    if (initialValue) i.val(initialValue);

    // set listeners
    i.blur(onEntryBlur);
    i.click(onAtomClick);
    i.focus(onEntryFocus);
    i.keypress(onEntryKeypress);
    i.keydown(onEntryKeydown);
    d.children('.hook').hover(function(){$(this).children('.in,.out,.poke').fadeIn(100);}, function() {$(this).children('.in,.out,.poke').fadeOut(500);});
    d.children('.hook').children('.in,.out,.poke').click(onHookClick);
    d.attr('draggable', true);
    i.on('dragenter', onDragEnterEntry);
    i.on('dragleave', onDragLeaveEntry);
    d.on('dragstart', onDragStart);
    d.on('dragend', onDragEnd);
    d.children('.close').click(onCloseClick);
    
    // data
    d.data('for', []);
    d.data('children', []);
    
    // file uploading
    d.append("<span class='fileinput-button'><span>...</span><input type='file' name='files[]'></span>");
    d.children('.fileinput-button').click(onFileInputClick).change(onFileInputChange);
    d
    return d;
}

function onUploadDone() {
    console.log($(this).contents());
}

function areaOnClick(event) {
    var x = event.pageX;
    var y = event.pageY;
    addAtom(x, y).children('input').focus();
}

function init() {
    area = $('#area');
    toaster = $('#toaster');
    toasterHeight = toaster.height();
    toaster.hide();
    area.click(areaOnClick);
    $(document).scrollTop(area.height() / 2 - $(window).height() / 2);
    $(document).scrollLeft(area.width() / 2 - $(window).width() / 2);
    defaultEntryWidth = $('#proto_entry').width();
    defaultEntryHeight = $('#proto_entry').height();
    $('#proto_entry').detach();
    setTimeout(function() { $("#killme").fadeOut(4000, function(){$(this).detach();}); }, 200);
    $('#hidden_upload').load(onUploadDone);
}
 
$(document).ready(init);
