var server = 'http://miellaby.selfip.net:8008/';
var area = null;
var animatePlease = true;

function dismissAtom(d) {
   dismissArrow(d);
}

function dismissArrow(d, direction /* false = down true = up */) {
   // guard double detach
   if (d.data('detached')) return;
   //// only detach loose arrow
   // if (d.data('children')) return;
   // for guard
   d.data('detached', true);

   if (d.hasClass('pairDiv')) {
       // propagate deletion to tail
       var t = d.data('tail');
       dismissArrow(t, true);

       // propagate deletion to head
       var h = d.data('head');
       dismissArrow(h, true);

   }

   // propagate to loose child
   if (!direction) { //  when going "down"
       f = d.data('for'); 
       if (f) {
          dismissArrow(f);
       }
  }

   // fold children
   var children = d.data('children');
   if (children) {
       for (var i = 0; i < children.length; i++) {
            var c = children[i];
            // var subchildren = c.data('children');
            // TODO show unfold buttons
            if (c.data('head')[0] == d[0]) {
                c.children('.headDiv').height(0);
                continue;
            }
            if (c.data('tail')[0] == d[0]) {
                c.children('.tailDiv').height(0);
                continue;
            }
       }
    }

    d.detach();
}

function turnAtomIntoPair(d) {
    if (d.data('children')) {
       throw new Exception("connected arrows are immutable");
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
    t.data('for', a);
    h.data('for', a);

    // if atom belonged to a loose arrow, one rewires to the obtained pair
    if (f) {
       newFor = rewirePair(f, d, a);
       a.data('for', newFor);
    }

    
    // if atom had content, it went to the pair tail (see addAtom call)
    if (v !== '') {
       // and one builds a second pair
       turnAtomIntoPair(h);
    } else {
        t.children('input').focus();
    }

   // detach atom
    d.detach();
}

// find an editable atom into a tree
function findNextInto(d) {
    if (d.hasClass('atomDiv')) { // atom
        return d.data('for') ? d /* found */: null;
    } else { // pair
       // check in tail then in head
       var n = findNextInto(d.data('tail'));
       if (!n) n = findNextInto(d.data('head'));
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
       return encodeURIComponent(d.children('input').val());
    } else { // pair ends URI
       tailUri = buildUri(d.data('tail'));
       headUri = buildUri(d.data('head'));
       return '/' + tailUri + '+' + headUri;
    }
}

function turnInputIntoText(d) {
    var i = d.children('input');
    var v = i.val() || 'EVE';
    var w0 = i.width();
    d.addClass('kept');
    d.children('.close').show().click(onCloseClick);
    i.detach();
    var s = $("<span style='display: inline-block;'></span>").text(v).appendTo(d);
    var w = s.width();
    s.css({width: (w < 100 ? 100 : w) + 'px', 'text-align': 'center'});
    d.css('left', (w < w0 ? '+=' + ((w0 - w) / 4) : '-=' + ((w - w0) / 4)) + 'px');
}

function unlooseArrow(d, parent) {
    // already connected arrow?
    if (parent && !d.data('for')) return;

    if (d.hasClass('atomDiv')) { // atom
       // make it immutable
       turnInputIntoText(d);
    }

    if (d.hasClass('pairDiv')) { // pair?
       // unloose its end
       unlooseArrow(d.data('tail'), d);
       unlooseArrow(d.data('head'), d);
    }

    d.removeData('for');
    var children = [parent];
    d.data('children', children);
}

function assimilateArrow(d) {
    var uri = buildUri(d);
    // assimilation request
    var request = $.ajax({url: server + uri + '?iDepth=0', xhrFields: { withCredentials: true }});
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
       var instruction = server + 'root/position/' + d.data('url') + '/' + parseInt(d.left) + '+' + parseInt(d.top);
       $.ajax({url: instruction, xhrFields: { withCredentials: true }});
    });
}

function recordArrow(d) {
    assimilateArrow(d);
    
    // unloose arrow
    unlooseArrow(d);
}

// search for the next editable atom
function findNext(d, from, leftOnly) {
   var f = d.data('for');
   if (d.hasClass('pairDiv') && d.data('head')[0] != from[0]) { // search into head
      var n = findNextInto(d.data('head'));
      if (n) return n;
   }
   
   if (f) { // search into loose children
        var n = findNext(f, d, leftOnly);
        if (n) return;
   }
   
  if (!leftOnly && d.hasClass('pairDiv') && d.data('tail')[0] != from[0]) { // search into tail
      var n = findNextInto(d.data('tail'));
      if (n) return n;
   }

   // search fail: no editable
   return null;
}

function onEntryKeypress(e) {
    var next = null;
    console.log(e);

    if (e.which == 47 /* / */) {
        var d = $(this).parent();
        e.preventDefault();
        turnAtomIntoPair(d);
        return false;
    } else if (e.which == 43 /* + */) {
        var d = $(this).parent();
        e.preventDefault();
        var f = d.data('for');
        next = findNext(f, d, true /* leftOnly */);
        if (next) {
            next.children('input').focus();
        } else { // overloading + ==> open a pair with the whole loose child arrow
            var ff;
            while (ff = f.data('for')) {
               f = ff;
            }
            var a = leave(f, false /* out */);
            f.data('for', a);
        }
     } else if (e.which == 13 /* return */) {
        var d = $(this).parent();
        e.preventDefault();
        // record the whole child arrow
        var f;
        while (f  = d.data('for')) {
           d = f;
        }
        recordArrow(d);
        $(this).blur();
    }
}

function onEntryFocus(e) {
    var i = $(this);
    var d = i.parent();
    if (d.data('children')) {
       i.blur();
    }
    return true;
}

function onEntryClick(e) {
    var d = $(this).parent();
    d.css('marginTop','-10px').animate({marginTop: 0});
    //e.preventDefault();
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
   if (inOut) {
      var n = addAtom(p.left - defaultEntryWidth - 100 - c.x, p.top + d.height() / 2 - c.y);
      var a = addPair(n, d, animatePlease);
   } else {
      var n = addAtom(p.left + d.width() + 100 + c.x, p.top + d.height() / 2 + c.y);
      var a = addPair(d, n, animatePlease);
   }
   n.data('for', a);
   n.children("input").focus();
   return a;
}

function onHookClick(e) {
   var i = $(this);
   var d = i.parent().parent();
   if (i.hasClass('in'))
      leave(d, true);
   else (i.hasClass('out'))
      leave(d, false);
   return false;
}

function onCloseClick(e) {
    var d = $(this).parent();
    dismissAtom(d);
    return false;
}

var toaster;
function toast(source, text) {
    var p = source.position();
    var w = source.width();
    toaster = toaster || $('#toaster');
    toaster.animate({ left: p.left + w / 2, top: p.top - source.height()}, 50, 'linear', function() { toaster.show().text(text).css('left', '-=' + (toaster.width()/2)+'px').css('opacity', 0.5).animate({ top: "-=50px", opacity: 0}, 2000);});
    
}

function recordOrDetachAtom(d) {
    if (!d.data('for') && !d.data('url')) {
        setTimeout(function() {
            if (!d.data('for') && !d.data('url'))
                 dismissAtom(d);
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
        next = findNext(f, d);
        if (next) {
            next.children('input').focus();
        } else {
            var ff;
            while (ff = f.data('for')) {
               f = ff;
            }
            var a = leave(f, false /* out */);
            f.data('for', a);
        }
    }
}

function rewirePair(a, oldOne, newOne) {
   var tail = (a.data('tail')[0] === oldOne[0] ? newOne : a.data('tail'));
   var head = (a.data('head')[0] === oldOne[0] ? newOne : a.data('head'));
   var newA = addPair(tail, head);
   var f = tail.data('for');
   if (f && f[0] === a[0])
      tail.data('for', newA);
   f = head.data('for');
   if (f && f[0] == a[0])
      head.data('for', newA);

   var children = oldOne.data('children');
   if (children) {
       for (var i = 0; i < children.length; i++) {
           var c = children[i];
           if  (c[0] == a[0]) {
                children.splice(i, 1);
                break;
           }
       }
       children = newOne.data('children') || [];
       children.push(newA);
       newOne.data('children', children);
   }
   
   var f = a.data('for');
   if (f) {
      newFor = rewirePair(f, a, newA);
      newA.data('for', newFor);
   }
   
   a.detach();

   return newA;
}

function moveArrow(e) {
    var d= $(this);
    console.log(e);
    //d.css('opacity', 1).css('left', d.position().left  + e.originalEvent.offsetX +'px').css('top', d.position().top + e.originalEvent.offsetY +'px'); 
    d.css('opacity', 1).css('left', e.originalEvent.pageX +'px').css('top', e.originalEvent.pageY +'px'); 
    var children = d.data('children');
    if (children) {
       for (var i = 0; i < children.length; i++) {
           var c = children[i];
           rewirePair(c, d, d)
       }
    }
}

function addPair(tail, head, animate) {
    var p0 = tail.position();
    var p1 = head.position();
    var x0 = p0.left + tail.width() / 2 + 3;
    var y0 = p0.top + tail.height();
    var x1 = p1.left + head.width() / 2 - 3;
    var y1 = p1.top + head.height();
    var pairDiv = $("<div class='pairDiv'><div class='tailDiv'><div class='tailEnd'></div></div><div class='headDiv'></div><div class='hook'><div class='in'>&uarr;</div> <div class='out'>&darr;</div></div></div>");
    area.append(pairDiv);
    pairDiv.css('left', x0 + 'px');
    pairDiv.css('top', Math.min(y0, y1) + 'px');
    pairDiv.css('width', (x1 - x0) + 'px');
    pairDiv.css('height', (Math.abs(y1 - y0) + 50) + 'px');
    if (animate){
        pairDiv.hide();
        pairDiv.children('.tailDiv').animate({'height': (50 + ((y0 < y1) ? y1 - y0 : 0)) + 'px'});
        pairDiv.children('.headDiv').animate({'height': (50 + ((y1 < y0) ? y0 - y1 : 0)) + 'px'});
        pairDiv.fadeIn(500);
    } else {
        pairDiv.children('.tailDiv').css({'height': (50 + ((y0 < y1) ? y1 - y0 : 0)) + 'px'});
        pairDiv.children('.headDiv').css({'height': (50 + ((y1 < y0) ? y0 - y1 : 0)) + 'px'});
    }
    pairDiv.children('.hook').hover(function(){$(this).children('.in,.out').fadeIn(100);}, function() {$(this).children('.in,.out').fadeOut(500);});
    pairDiv.children('.hook').children('.in,.out').click(onHookClick);
    pairDiv.data('tail', tail);
    pairDiv.data('head', head);
    pairDiv.attr('draggable', true);
    pairDiv.on('dragstart', function(e) { $(this).css('opacity', 0.4); });
    pairDiv.on('dragend', moveArrow);
    return pairDiv;
}

function addAtom(x, y, initialValue) {
    var d = $("<div class='atomDiv'><input type='text'></input><div class='close'>&diams;</div><div class='hook'><div class='in'>&uarr;</div> <div class='out'>&darr;</div></div></div>");
    area.append(d);
    if (animatePlease) {
        d.css('opacity', 0.1).animate({ opacity: 1});
    }
    d.css('left', x + 'px');
    d.css('top', y + 'px');
    var i = d.children('input');
    if (initialValue) i.val(initialValue);
    i.blur(onEntryBlur);
    i.click(onEntryClick);
    i.focus(onEntryFocus);
    i.keypress(onEntryKeypress);
    i.keydown(onEntryKeydown);
    d.children('.hook').hover(function(){$(this).children('.in,.out').fadeIn(100);}, function() {$(this).children('.in,.out').fadeOut(500);});
    d.children('.hook').children('.in,.out').click(onHookClick);
    d.attr('draggable', true);
    d.on('dragstart', function(e) { $(this).css('opacity', 0.4); });
    d.on('dragend', moveArrow);
    return d;
}

function areaOnClick(event) {
    var x = event.pageX;
    var y = event.pageY;
    addAtom(x, y).children('input').focus();
}

function init() {
    area = $('#area');
    area.click(areaOnClick);
    $(document).scrollTop(area.height() / 2 - $(window).height() / 2);
    $(document).scrollLeft(area.width() / 2 - $(window).width() / 2);
    defaultEntryWidth = $('#proto_entry').width();
    $('#proto_entry').detach();
    setTimeout(function() { $("#killme").fadeOut(4000, function(){$(this).detach();}); }, 200);
}
 
$(document).ready(init);
