/**
 * Compute an hashcode
 * @this {String}
 * @return {number} hash
 * @author http://stackoverflow.com/users/333214/kimkha
 */
String.prototype.hashCode = function () {
    var hash = 0;
    if (this.length === 0) return hash;
    for (var i = 0; i < this.length; i++) {
        var character = this.charCodeAt(i);
        hash = ((hash << 5) - hash) + character;
        hash = hash & hash; // Convert to 32bit integer
    }
    return hash;
};

/**
 * Creates an instance of {Arrow}.
 * @class {Arrow} 'Everything is an arrow'. That is an oriented pair of {Arrow}s.
 * @constructor
 * @this {Arrow}
 * @param {Arrow} t tail
 * @param {Arrow} h head
 * @param {number} hc The hashcode for this arrow.
 */
function Arrow(t, h, hc) {
    /** tail */
    this.tail = t;

    /** head */
    this.head = h;

    /** hashcode */
    this.hc = hc;

    /** reference counters */
    this.refCount = 0;

    /** root flag */
    this.rooted = false;

    /** digest */
    this.digest = null; // TODO digest >=< URi (but for for atom)

    /** canonical uri */
    this.uri = null;
    
    /** payload (for outer programmatic purpose) */
    // this.payload = null;
    
    /** cc (commit mark) */
    // this.cc = null;
    
    /** views (for outer programmatic purpose) */
    // this.views = null;
    
    Arrow.loose.push(this);
}

// Some constants

/** Filter value for getChildren */
Arrow.FILTER_NOTHING = 0;

/** Filter value for getChildren */
Arrow.FILTER_UNROOTED = 1;

/** Filter value for getChildren */
Arrow.FILTER_OUTGOING = 2;

/** Filter value for getChildren */
Arrow.FILTER_INCOMING = 4;


/** indexes size */
Arrow.indexSize = 2000; // "Nobody Will Ever Need More Than 640k RAM!"

/** default public server base URL */
Arrow.publicEntrelacsServer = "https://entrelacs-miellaby.c9.io/"; // Default public server

// indexes (as maps of lists of arrows)
//
//      Please note that implementations at lower levels merge
//    arrow storage and indexes in a single memory space.
//
/** 'by definition' index */
Arrow.defIndex = new Array(Arrow.indexSize);

/** 'by head' index */
Arrow.headIndex = new Array(Arrow.indexSize);

/** 'by tail' index */
Arrow.tailIndex = new Array(Arrow.indexSize);

/** 'by digest' index */
Arrow.digestIndex = {};

/** 'by url' index */
Arrow.urlIndex = {};

/** changes log */
Arrow.changelog = [];

/** loose arrows log */
Arrow.loose = [];

/** commit count */
Arrow.cc = 0;

/** Hook code here */    
Arrow.listeners = [];


$.extend(Arrow.prototype, {
    /**
     * Connects an arrow to its both ends
     * @this {Arrow}
     */
    connect: function () {

        // to head
        if (this.head) {
            // ihc: hash code for index
            var ihc = this.head.hc % Arrow.indexSize;
            var i = Arrow.headIndex[ihc];
            if (!i) i = Arrow.headIndex[ihc] = [];
            i.push(this);
            if (this.head.isLoose()) this.head.connect();
            this.head.refCount++;
        }

        // to tail
        if (this.tail) {
            // ithc: hash code for index
            var ithc = this.tail.hc % Arrow.indexSize;
            var it = Arrow.tailIndex[ithc];
            if (!it) it = Arrow.tailIndex[ithc] = [];
            it.push(this);
            if (this.tail.isLoose()) this.tail.connect();
            this.tail.refCount++;
        }

        Arrow.loose.splice(Arrow.loose.indexOf(this), 1);
    },

    /**
     * Disconnects an arrow from its both ends
     * @this {Arrow}
     */
    disconnect: function () {
        // from head
        if (this.head) {
            var ihc = this.head.hc % Arrow.indexSize;
            var i = Arrow.headIndex[ihc];
            i.splice(i.indexOf(this), 1);
            this.head.refCount--;
            if (this.head.isLoose()) this.head.disconnect();
        }

        // from tail
        if (this.tail) {
            var ithc = this.tail.hc % Arrow.indexSize;
            var it = Arrow.tailIndex[ithc];
            it.splice(it.indexOf(this), 1);
            this.tail.refCount--;
            if (this.tail.isLoose()) this.tail.disconnect();
        }

        Arrow.loose.push(this);
    },

    /**
     * Roots an arrow
     * @this {Arrow}
     */
    root: function () {
        if (this.hc === undefined) return; // GC-ed!
        if (this.rooted) return;
        if (this.isLoose()) {
            this.connect();
        }
        this.rooted = true;
        Arrow.changelog.push(this);
        Arrow.callListeners(this);
        return this;
    },

    /**
     * Unroots an arrow
     * @this {Arrow}
     */
    unroot: function () {
        if (this === Arrow.eve) return; // Not EVE
        if (this.hc === undefined) return; // GC-ed!
        if (!this.rooted) return;
        this.rooted = false;
        if (this.isLoose()) {
            this.disconnect();
        }
        Arrow.changelog.push(this);
        Arrow.callListeners(this);
        return this;
    },

    /**
     * @this {Arrow}
     * @return is this arrow rooted?
     */
    isRooted: function () {
        return this.rooted;
    },

    /**
     * @this {Arrow}
     * @return boolean is this arrow loose?
     */
    isLoose: function () {
        return !this.rooted && !this.refCount;
    },

    /**
     * @this {Arrow}
     * @return boolean is this arrow atomic?
     */
    isAtomic: function () {
        return undefined;
    },

    /**
     * @this {Arrow}
     * @return {Arrow} the arrow tail
     */
    getTail: function () {
        return this.tail;
    },

    /**
     * @this {Arrow}
     * @return {Arrow} the arrow head
     */
    getHead: function () {
        return this.head;
    },

    /**
     * @this {Arrow}
     * @return {Function} a Functor to iterate all children
     *   (returns null when over)
     */
    getChildren: function(filters) {
        var children = [];

        // incoming
        var i = Arrow.headIndex[this.hc % Arrow.indexSize];
        if (i) Array.prototype.push.apply(children, i);

        // outgoing
        i = Arrow.tailIndex[this.hc % Arrow.indexSize];
        if (i) Array.prototype.push.apply(children, i);

        i = null; // prevent polluting the closure
        var indice = 0;
        var parent = this;
        return function () {
            // returns a real child, looping through false positives
            while (indice < children.length) {
                var a = children[indice++];
                if (a.getTail() != parent && (filters & Arrow.FILTER_INCOMING)
                    || a.getHead() != parent && (filters & Arrow.FILTER_OUTGOING))
                    continue;
                if ((filters & Arrow.FILTER_UNROOTED) && !a.isRooted())
                    continue;
                if (a.getTail() != parent && a.getHead() != parent)
                    continue;
                return a;
            }
            return null;
        };
    },

    /** Returns one child or null if no child
     * @this {Arrow}
     * @return {Arrow} 
     */
    getChild: function(filters) {
        return this.getChildren(filters)();
    },
    
    /**
     * @this {Arrow}
     * @return a list of partners (context-rooted children)
     */
    getPartners: function() {
        var child, iterator = this.getChildren();
        var list = Arrow.eve;
        while ((child = iterator()) != null) {
            if (child.isRooted()) {
                list = Arrow.pair(child, list);
            }
        }
        return list;
    },
    
    
     /** Load some payload into an arrow
     * @this {Arrow}
     * @param keyOrMap (a key string or a key-value mapping object)
     * @param value to bind the key to (only relevant if first parameter is a key string)
     */
    set: function(keyOrMap, value) {
        this.payload || (this.payload = {});
       
        if (typeof keyOrMap == 'object') {
            for (var k in keyOrMap) {
                keyOrMap.hasOwnProperty(k) && (this.payload[k] = keyOrMap[k]);
            }
        } else {
            this.payload[keyOrMap] = value;
        }
    },
    
     /** Get some payload out of an arrow
     * @this {Arrow}
     */
    get: function(key) {
        return this.payload ? this.payload[key] : undefined;
    },
    
    /** Delete some payload out of an arro
     * @this {Arrow}
     */
    reset: function(key) {
        delete this.payload[key];
    },
    
    /** bind digest
     * @this {Arrow}
     */
     bindDigest: function(digest) {
         if (this.digest && this.digest != digest) {
             throw new Error("digest changes!");
         }
         this.digest = digest;
         var existingArrow = Arrow.digestIndex[digest];
         if (existingArrow !== undefined && existingArrow != this) {
             existingArrow.replaceWith(this);
         }
         Arrow.digestIndex[digest] = this;
     },

    /**
     * @this {Atom}
     * @when a placeholder is solved in an arrow definition,
     * one needs to replace it with the right arrow
     */
    replaceWith: function(a) {
        console.log("replace " + Arrow.serialize(this) + " with " + Arrow.serialize(a));
        if (a.hc === undefined) {
            console.log("won't replace with GC-ed arrow");
            return;
        }
        
        
        var digest  = this.digest;
        if (digest && !a.digest) {
            delete Arrow.digestIndex[digest]; // to avoid recursion
            a.bindDigest(digest);
        }

        Arrow.callListeners(this, a);

         // recursive call on all children
        var child, iterator = this.getChildren();
        var children = [];
        while ((child = iterator()) != null) {
            children.push(child);
        }
        
        for (var i = 0; i < children.length; i++) {
            child = children[i];
            if (child.head === this) {
                if (child.tail === this) {
                    child.replaceWith(Arrow.pair(a, a));
                } else {
                    child.replaceWith(Arrow.pair(child.tail, a));
                }
            } else if (child.tail === this) {
                child.replaceWith(Arrow.pair(a, child.head));
            }
        }
        
        this.unroot(); // ensure disconnection
        
        // (this) will be erased by next GC
        return a;
    }

});

/** Advertise change */
Arrow.callListeners = function (arrow, effect) {
    Arrow.listeners.forEach(function(cb) { cb(arrow, effect); });
};

/**
 * Creates an instance of {Atom}.
 * @class An atom is an arrow which corresponds to a given piece of data.
 * @this {Atom}
 * @extends {Arrow}
 * @constructor
 * @param {string} body a toString() compatible Javascript object
 * @param {number} hc The hashcode for this atom
 */
function Atom(body, hc) {
    if (hc === undefined) hc = String(body).hashCode();

    Arrow.call(this, undefined, undefined, hc);
    // notice: atom ends should be itself.
    // but undefined ends are prefered to not trick JS GC.

    this.body = body;
}

$.extend(Atom.prototype, Arrow.prototype, {
    /**
     * @this {Atom}
     * @return true because this arrow is atomic
     */
    isAtomic: function () {
        return true;
    },

    /**
     * connect an atom
     * @this {Atom}
     */
    connect: function () {
        Arrow.loose.splice(Arrow.loose.indexOf(this), 1);
    },

    /**
     * disconnect an atom
     * @this {Atom}
     */
    disconnect: function () {
        Arrow.loose.push(this);
    },

    /**
     * @this {Atom}
     * @return this because this arrow is atomic
     */
    getTail: function () {
        return this;
    },

    /**
     * @this {Atom}
     * @return this because this arrow is atomic
     */
    getHead: function () {
        return this;
    },

    /**
     * @this {Atom}
     * @return the atom body
     */
    getBody: function () {
        return this.body;
    },
    
    replaceWith: function(a) {
        throw new Error('atom are not replacable.');
    }
});



/**
 * Creates an instance of {Pair}.
 * @class a pair is an non-atomic arrow
 * @constructor
 * @this {Pair}
 * @extends {Arrow}
 * @param {Arrow} t tail
 * @param {Arrow} h head
 * @param {number} hc hashcode
 */
function Pair(t, h, hc) {
    if (t === undefined || h === undefined)
        throw 'what?';
    if (hc === undefined) hc = (String(t.hc) + String(h.hc)).hashCode();
    Arrow.call(this, t, h, hc);
}

$.extend(Pair.prototype, Arrow.prototype, {
    /**
     * @this {Pair}
     * @return false because this arrow is not atomic
     */
    isAtomic: function () {
        return false;
    },
});



/**
 * Creates an instance of {Placeholder}.
 * @class a placeholder for an existing arrow that we only know
 *        by its temporary URL on a remote Entrelacs server
 * @constructor
 * @this {Placeholder}
 * @extends {Arrow}
 * @param {string} url
 */
function Placeholder(server, url, hc) {
    if (url[0] !== '$')
        throw 'bad url';
    if (hc === undefined) hc = (server + url).hashCode();
    Arrow.call(this, undefined, undefined, hc);
    this.server = server;
    this.url = url;
    if (url[1] === 'H') {
        if (Arrow.digestIndex[url])
            throw new Error('why a placeholder? arrow is known!');
        this.bindDigest(url);
    }
}

$.extend(Placeholder.prototype, Arrow.prototype, {
});



/**
* Forget everything
*/
Arrow.wipe = function () {
  Arrow.defIndex = [];
  Arrow.headIndex = [];
  Arrow.tailIndex = [];
  Arrow.digestIndex = {};
  Arrow.urlIndex = {};
  Arrow.changelog = [];
  Arrow.loose = [];
  Arrow.cc = 0;
};


/**
 * Eve: the most simplest arrow, here considered equivalent to an empty-string based atom
*/
Arrow.eve = new Atom("", "".hashCode()).root();
Arrow.eve.rooted = true;
Arrow.changelog = [];


Arrow.eveIfNull = function(v) {
    if (!v)
        return Arrow.eve;
    else
        return v;
};

/**
 * Finds or builds the unique atomic arrow corresponding to a piece of data
 * @param {boolean} test existency test only => no creation
 */
Arrow.atom = function (body, test) {
    var atom;
    if (!body.length) return Arrow.eve;
    
    var hc = String(body).hashCode();
    var a = hc % Arrow.indexSize;
    var i = 0;
    var l = Arrow.defIndex[a];
    if (!l) l = Arrow.defIndex[a] = [];

    for (i = 0; i < l.length && (atom = l[i]) && !(atom.hc == hc && atom.body == body); i++);
    if (i < l.length) return atom;

    if (test) return undefined;
    
    l.push(atom = new Atom(body, hc));
    return atom;
};

/**
 * Finds or builds the unique compound arrow linking 't' (its tail) to 'h' (its head)
 * @param {Arrow} t the tail
 * @param {Arrow} h the head
 * @param {boolean} test existency test only => no creation
 */
Arrow.pair = function (t, h, test) {
    var pair;
    var hc = (String(t.hc) + String(h.hc)).hashCode();
    var a = hc % Arrow.indexSize;
    var l = Arrow.defIndex[a];
    var i = 0;

    if (!l) l = Arrow.defIndex[a] = [];    
    for (i = 0; i < l.length && (pair = l[i]) && !(pair.hc == hc && pair.tail === t && pair.head === h); i++);
    if (i < l.length) return pair;
    
    if (test) return undefined;
    
    l.push(pair = new Pair(t, h, hc));
    return pair;
};

/**
 * Finds or builds an arrow placeholder for some remote URL
 * param {url} server url / identifier
 * param {url} uri that we haven't yet resolved to a local arrow
 * @return {Arrow}
 */
Arrow.placeholder = function (server, url, test) {
    var hc = String(server + url).hashCode(); // useless, kept for consistency

    if (url[0] !== '$') {
        throw new Error('placeholder are for URL/URI, not: ' + url);
    }
    var existing = Arrow.urlIndex[url];
    if (existing) {
        return existing;
    }
    if (url[1] === 'H') { // URI (digest)
        existing = Arrow.digestIndex[url];
        if (existing) {
            console.log('Arrow with same digest exists. No placeholder for ' + url);
            return existing;
        }
        if (test) return undefined;
        return new Placeholder(server, url, hc); //< will add to digestIndex
    } else { // session-bound URL
        
        if (test) return undefined;
        return (Arrow.urlIndex[url] = new Placeholder(server, url, hc));
    }
};


/**
 * Parses an URI/URL to find or build its corresponding unique arrow
 * @param {string} Entrelacs-Compatible URI/URL
 * @param {string} Entrelacs server URL in case of partially defined URI
 * @return {Arrow}
 */
Arrow.decodeURI = function () {
    // We define it inside a closure to keep bits private.

    // Simple recursive descent parser

    var at, // The index of the current character
    ch, // The current character
    text, // the text being parsed
    baseUrl, // the remote server base URL
    test, // check only URL
    
    refToString = function () {
        return this.ref;
    },

    Ref = function (string) {
        this.ref = string;
        this.toString = refToString;
    },

    error = function (m) { // when something is wrong.
        throw {
            name: 'SyntaxError',
            message: m,
            at: at,
            text: text
        };
    },

    next = function () { // Get the next character
        ch = text.charAt(at);
        at += 1;
        return ch;
    },

    ref = function () { // Parse a ref.
        var string = '';
        if (ch === '$') {
            // stop at + or / characters.
            while (ch !== '+' && ch !== '/' && ch !== '') {
                string += ch;
                next();
            }
            if (string[1] === 'H') {
                console.log('parsing a digest: ' + string);
            }
            return Arrow.placeholder(baseUrl, string, test); //< may find an arrow by digest
        }
    },

    utf8_decode = function (utftext) {
        var string = "";
        var i = 0;
        var c =0, c2 = 0, c3 = 0;
   
        while ( i < utftext.length ) {
   
             c = utftext.charCodeAt(i);
     
             if (c < 128) {
                  string += String.fromCharCode(c);
                  i++;
             }
             else if((c > 191) && (c < 224)) {
                  c2 = utftext.charCodeAt(i+1);
                  string += String.fromCharCode(((c & 31) << 6) | (c2 & 63));
                  i += 2;
             }
             else {
                  c2 = utftext.charCodeAt(i+1);
                  c3 = utftext.charCodeAt(i+2);
                  string += String.fromCharCode(((c & 15) << 12) | ((c2 & 63) << 6) | (c3 & 63));
                  i += 3;
             }
   
        }
   
        return string;
	 },
    
   atom = function () { // Parse an atom.
        var hex, i, ff, string = '';

        // stop at + or / characters.
        while (ch !== '+' && ch !== '/' && ch !== '') {
            if (ch === '%') {
                ff = 0;
                for (i = 0; i < 2; i++) {
                    hex = parseInt(next(), 16);
                    if (!isFinite(hex)) {
                        error('not hexadecimal');
                    }
                    ff = ff * 16 + hex;
                }
                string += String.fromCharCode(ff);
            } else {
                string += ch;
            }
            next();
        }
        string = utf8_decode(string);
        return Arrow.atom(string);
    },

    arrow, // Place holder for the value function.

    pair = function () { // Parse a pair.
        var tail, head;
        if (ch === '/') {
            next();
            tail = arrow();
            if (ch === '+') next();
            head = arrow();
            if (test && (!tail || !head)) return undefined;
            return Arrow.pair(tail, head);
        }
    };

    arrow = function () { // Parse a pair or an atom
        return (ch === '/' ? pair() : (ch === '$' ? ref() : atom()));
    };

    // Return the overall parsing function
    return function (uri, serverUrl, checkOnly) {
        baseUrl = serverUrl || Arrow.publicEntrelacsServer;
        test = checkOnly;
        text = uri;
        at = 0;
        next();
        return arrow();
    };
}();

/**
 * Get a valid URI from an arrow definition
 */
Arrow.serialize = function(arrow) {
    if (arrow.url) { // placeholder
        return arrow.url;
    } else if (arrow.digest) {
        return arrow.digest;
    } else if (arrow.isAtomic()) {
        return encodeURIComponent(arrow.getBody());
    } else {
        var tailUrl = Arrow.serialize(arrow.getTail());
        var headUrl = Arrow.serialize(arrow.getHead());
        return '/' + tailUrl + '+' + headUrl;
    }
};

/**
 * Garbage collector
 */
Arrow.gc = function () {
    // on a lower-level system, the arrow GC can't run between 2 commits as it may free unrooted arrows and its descendants before the changes have been commited
    // if (Arrow.changelog.length) return;

    for (var i = 0; i < Arrow.loose.length; i++) {
        var lost = Arrow.loose[i];
        if (lost.hc === undefined) throw "double gc?";
        if (lost.url) {
            delete Arrow.urlIndex[lost.url];
        } else {
            var a = lost.hc % Arrow.indexSize;
            var l = Arrow.defIndex[a];
            l.splice(l.indexOf(lost), 1);
            if (a.digest) {
                delete Arrow.digestIndex[a.digest];
            }
        }
        lost.hc = undefined; // mark arrow as GC-ed.
        Arrow.callListeners(lost, null);
    }
    Arrow.loose.splice(0, Arrow.loose.length);
};



/**
 * Local changes commit
 ** Note: it might be perfectly feasible to commit on several servers at once
 */
Arrow.commit = function (entrelacs) {
    Arrow.cc++;
    
    for (var i = 0; i < Arrow.changelog.length; i++) {
        var changed = Arrow.changelog[i];
        if (changed.cc == Arrow.cc) continue; // already done
        changed.cc = Arrow.cc;
        if (changed.isRooted()) {
            entrelacs.root(changed);
        } else {
            entrelacs.unroot(changed);
        }
    }
    var promise = entrelacs.commit();

    promise.done(function () {
        Arrow.changelog.splice(0, Arrow.changelog.length);
        Arrow.gc();
    });

    return promise;
};

/**
 * Reset placeholders and descendants
 */
Arrow.resetPlaceholders = function (serverUrl) {
    console.log('resetPlaceholders');

    var newIndex = {};

    var unrootDescendants = function(a) {
        a.unroot();
        var child, iterator = a.getChildren();
        while ((child = iterator()) != null) {
            unrootDescendants(child);
        }
    };
    
    for (var url in Arrow.urlIndex) {
        var placeholder = Arrow.urlIndex[url];
        if (placeholder.server == serverUrl) {
            unrootDescendants(placeholder);
        } else {
            newIndex[url] = placeholder;
        }
    }
    
    Arrow.urlIndex = newIndex;
    Arrow.callListeners(null); // FIXME
};

/**
 * Creates an instance of {Entrelacs}
 * @class {Entrelacs} A proxy object in front a remote Entrelacs server.
 * @constructor
 * @this {Entrelacs}
 * @param {string} serverUrl server base URL
 */

function Entrelacs(serverUrl) {
    /** server base URL */
    this.serverUrl = serverUrl || Arrow.publicEntrelacsServer;

    /** last known cookie, */
    this.lastCookie = null;
    
    /** current promise */
    this.chain = $.when({});
}

$.extend(Entrelacs.prototype, {

    /**
     * reset proxy session
     */
    reset: function () {
        console.log('Entrelacs reset');

        Arrow.resetPlaceholders(this.serverUrl);
    },

    /** 
     * check cookie and detect session loss
     */
    checkCookie: function () {
        if (!$.cookie) return;
        var currentCookie = $.cookie('session');
        if (currentCookie != this.lastCookie) {
            console.log('session loss');
            // session is lost, invalidate all URI
            this.lastCookie = currentCookie;
            this.reset();
        }
    },

    /**
     * get a canonical URI (full def or digests) for an arrow
     * @param {Arrow}
     * @return {JQuery.Promise}
     */
    getCanonicalUri: function (a, secondTry) {
        var promise, futur;
        var self = this;
        
        if (a.uri) {
            return $.when(a.uri);
        } else if (a.url) { // placeholder
            var openPromise = self.open(a);
            promise = openPromise.pipe(function(opened) {
                return self.getCanonicalUri(opened);             
            });
            // no need to try twice (already in open)
            return promise;
            
        } else if (a.isAtomic()) {
            if (a.getBody().length < 99) {
                a.uri = Arrow.serialize(a);
                return $.when(a.uri);
            } else {
                // ' query /+a?iDepth=2 to get blob digests
                // chain not needed
                var bodyUri = encodeURIComponent(a.getBody()); // TODO POST content
                var req = self.serverUrl + '/escape/+' + bodyUri + '?iDepth=2';
                promise = $.ajax({url: req, dataType: "text", xhrFields: { withCredentials: true }});
                promise.done(function (uri) {
                   self.checkCookie();
                });
                promise = promise.pipe(function (uri) { // uri == /+$Hxxxx
                    uri = uri.substring(2);
                    a.uri = uri;
                    a.bindDigest(uri);
                    return uri;
                });

                if (!secondTry) {
                    promise = promise.pipe(null, function() {
                        self.reset();
                        return self.getCanonicalUri(a, true);
                    });
                }

                return promise;
            }
        
        } else { // pair
            var pt = self.getCanonicalUri(a.getTail());
            var ph = self.getCanonicalUri(a.getHead());
            
            futur = function () {
                a.uri = '/' + a.getTail().get(self.uriKey) + '+' + a.getHead().get(self.uriKey);
                return a.uri;
            };
            
            return $.when(pt, ph).pipe(futur);
        }



    },

    /**
     * root an arrow
     * @param {Arrow}
     * @return {JQuery.Promise}
     */
    root: function (a, secondTry) {
        var isAtomic = a.isAtomic();
        var uri = Arrow.serialize(a);
        var self = this;
        // TODO linkTailWithHead : laborous; "linkify" might be better ; link to be renamed linker
        var operator = (isAtomic ? "root" : "linkTailWithHead");
        var req = self.serverUrl + '/' + operator + '/escape+' + uri;
        var promise = self.chain.pipe(function () {
            return $.ajax({url: req, dataType: "text", xhrFields: { withCredentials: true }});
        });
        promise.done(function (r) {
            self.checkCookie();
            if (a.hc === undefined) return; // a is GC-ed
        });

        if (!secondTry) {
            promise = promise.pipe(null, function() {
                self.reset();
                return self.root(a, true);
            });
            self.chain = promise;
        }
        
        return promise;
    },

    /**
     * unroot an arrow
     * @param {Arrow}
     * @return {JQuery.Promise}
     */
    unroot: function (a, secondTry) {
        var self = this;
        // TODO unlinkify
        var futur = function () {
            if (a.hc === undefined) {
                return $.when(null);   
            }
            var isAtomic = a.isAtomic();
            var uri = Arrow.serialize(a);
            var operator = (isAtomic ? "unroot" : "unlinkTailAndHead");
            var req = self.serverUrl + '/' + operator + '/escape+' + uri;
            return $.ajax({url: req, xhrFields: { withCredentials: true }});
        };
        var promise = self.chain.pipe(futur, futur);

        if (!secondTry) {
            promise = promise.pipe(null, function() {
                self.reset();
                return self.unroot(a, true);
            });
            self.chain = promise;
        }

        return promise;
    },

    /**
     * isRooted
     * @param {Arrow}
     * @return {Promise}
     */
    isRooted: function (a, secondTry) {
        var self = this;
        var futur = function () {
            if (a.hc === undefined) return $.when(null); // a is GC-ed
            var url = Arrow.serialize(a);
            var req = self.serverUrl + '/isRooted/escape+' + url;
            return $.ajax({url: req, dataType: "text", xhrFields: { withCredentials: true }});
        };
        var promise = self.chain.pipe(futur, futur);
        promise.done(function (r) {
            if (a.hc === undefined) return; // a is GC-ed
            self.checkCookie();
            if (r) {
                if (a.url) { // placeholder
                    a.replaceWith(self.decodeURI(a, r));
                }
                a.root();
            } else {
                a.unroot();
            }
        });
        
        if (!secondTry) {
            promise = promise.pipe(null, function() {
                self.reset();
                return self.isRooted(a, true);
            });
            self.chain = promise;
        }
        
        return promise;
    },

    /**
     * getChildren
     * @param {Arrow}
     * @return {JQuery.Promise}
     */
    getChildren: function (a, secondTry) {
        var self = this;
        var futur = function () {
            if (a.hc === undefined) return $.when(null); // a is GC-ed
            var url = Arrow.serialize(a);
            var req = self.serverUrl + '/childrenOf/escape+' + url + '?iDepth=10';
            return $.ajax({url: req, dataType: "text", xhrFields: { withCredentials: true }});
        };
        
        var promise = self.chain.pipe(futur, futur);
        var futur2 = function(arrowURL) {
            if (a.hc === undefined) return Arrow.eve; // a is GC-ed
            if (arrowURL === null) return Arrow.eve;
            self.checkCookie();
            var children = Arrow.decodeURI(arrowURL, self.serverUrl);
            return children;
        };
        
        promise = promise.pipe(futur2, futur2);
        
        if (!secondTry) {
            promise = promise.pipe(null, function() {
                self.reset();
                return self.getChildren(a, true);
            });
            self.chain = promise;
        }

        return promise;
    },

    /**
     * getPartners
     * @param {Arrow}
     * @return {JQuery.Promise}
     */
    getPartners: function (a, secondTry) {
        var self = this;
        var futur = function () {
            if (a.hc === undefined) return $.when(null); // a is GC-ed
            var uri = Arrow.serialize(a);
            var req = self.serverUrl + '/partnersOf/escape+' + uri + '?iDepth=10';
            return $.ajax({url: req, dataType: "text", xhrFields: { withCredentials: true }});
        };
        
        var promise = self.chain.pipe(futur, futur);
        
        promise = promise.pipe(function(arrowURL) {
            if (a.hc === undefined)
                return Arrow.eve; // a is GC-ed
            
            self.checkCookie();
            var list = Arrow.decodeURI(arrowURL, self.serverUrl);
            var partners = list;
            while (list !== Arrow.eve) {
                list.getTail().root();
                list = list.getHead();
            }
            return partners;
        });
        
       if (!secondTry) {
            promise = promise.pipe(null, function() {
                self.reset();
                return self.getPartners(a, true);
            });
            self.chain = promise;
        }

        return promise;
    },

    /**
     * invoke an arrow (evaluate) or an arrow URI
     * @param {Arrow} or {URI string}
     * @return {Promise}
     */
    invoke: function (a, secondTry) {
        var self = this;
        var futur = function () {
            if (typeof a === 'object' && a.hc === undefined) return $.when(null); // a is GC-ed
            var uri = (typeof a == "string") ? a : Arrow.serialize(a);
            var req = self.serverUrl + uri; // no escape this time
            return $.ajax({url: req, dataType: "text", xhrFields: { withCredentials: true }});
        };
        var promise = self.chain.pipe(futur, futur);

        promise = promise.pipe(function(arrowURIorText,  textStatus, xhr) {
            self.checkCookie();
            if (xhr.getResponseHeader("content-type") == "text/plain") {
                var atom = Arrow.atom(arrowURIorText);
                return atom;
            } else {
                var result = Arrow.decodeURI(arrowURIorText, self.serverUrl);
                return result;
            }
        });
        
        if (!secondTry) {
            promise = promise.pipe(null, function() {
                self.reset();
                return self.invoke(a, true);
            });
            self.chain = promise;
        }
        
        return promise;
    },

    /**
     * open/unfold/explore an arrow placeholder
     * @param {Arrow} p placeholder
     * @return {JQuery.Promise}
     */
    open: function (p, depth, secondTry) {
        var url;
        if (p.server != this.serverUrl)
            throw "bad placeholder";
        else
            url = p.url;

        depth = depth || 10;
        var self = this;
        // query /+p instead of p to avoid atom being returned as binary
        var futur = function () {
            var req = self.serverUrl + '/escape/+' + url + '?iDepth=' + depth;
            return $.ajax({url: req, xhrFields: { withCredentials: true }});
        };
        
        var promise = self.chain = self.chain.pipe(futur, futur);
        promise = promise.pipe(function (unfoldedUri) {
            self.checkCookie();
            // don't forget one looks for the head
            var unfolded = Arrow.decodeURI(unfoldedUri, self.serverUrl).getHead(); 
            p.replaceWith(unfolded);
            return unfolded;
        });

        if (!secondTry) {
            promise = promise.pipe(null, function() {
                self.reset();
                return self.open(p, depth, true);
            });
            self.chain = promise;
        }
        
        return promise;
    },

    /**
     * commit
     * @return {Promise}
     */
    commit: function (secondTry) {
        var self = this;
        var futur = function () {
            var req = self.serverUrl + '/commit+';
            return $.ajax({url: req, xhrFields: { withCredentials: true }});
        };
        var promise = self.chain.pipe(futur, futur);
        
        if (!secondTry) {
            promise = promise.pipe(null, function() {
                self.reset();
                return self.commit(true);
            });
            self.chain = promise;
        }
        
        return promise;
    }
});
