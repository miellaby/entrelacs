// constants
var indexSize = 2000;

// indexes
// note in lower-level AS implementations, arrow storage and indexes are all merged together
// into a single memory space
var defIndex[indexSize];
var headIndex[indexSize];
var tailIndex[indexSize];

// changes log
var changelog = [];

// loose arrows
var loose = [];

/**
 * Compute an hashcode
 * @this {String}
 * @return {number} hash
 * @author http://stackoverflow.com/users/333214/kimkha
 */
String.prototype.hashCode = function () {
    var hash = 0;
    if (this.length == 0) return hash;
    for (var i = 0; i < this.length; i++) {
        var character = this.charCodeAt(i);
        hash = ((hash << 5) - hash) + character;
        hash = hash & hash; // Convert to 32bit integer
    }
    return hash;
};

/**
 * Creates an instance of Arrow.
 * @class {Arrow} 'Everything is an arrow'. That is an oriented pair of {Arrow}s.
 * @constructor
 * @this {Arrow}
 * @param {Arrow} t tail
 * @param {Arrow} h head
 * @param {number} hc The hashcode for this arrow.
 */
function Arrow(h, t, hc) {
    this.head = h;
    this.tail = t;
    this.hc = hc;
    this.refCount = 0;
    this.rooted = false;
    loose.push(this);
}

$.extend(Arrow.prototype, {
    /**
     * Connects an arrow to its both ends
     * @this {Arrow}
     */
    connect: function () {

        // to head
        if (this.head) {
            // ihc: hash code for index
            var ihc = this.head.hc % indexSize;
            var i = headIndex[ihc];
            if (!i) i = headIndex[ihc] = [];
            i.push(this);
            this.head.refCount++;
            if (this.head.isLoose()) this.head.connect();
        }

        // to tail
        if (this.tail) {
            // ithc: hash code for index
            var ithc = this.tail.hc % indexSize;
            var it = tailIndex[ithc];
            if (!it) it = tailIndex[ithc] = [];
            it.push(this);
            this.tail.refCount++;
            if (this.tail.isLoose()) this.tail.connect();
        }

        loose.splice(loose.indexOf(this), 1);
    },

    /**
     * Disconnects an arrow from its both ends
     * @this {Arrow}
     */
    disconnect: function () {
        // from head
        if (this.head) {
            var ihc = this.head.hc % indexSize;
            var i = headIndex[ihc];
            i.splice(i.indexOf(this), 1);
            this.head.refCount--;
            if (this.head.isLoose()) this.head.disconnect();
        }

        // from tail
        if (this.tail) {
            var ithc = this.tail.hc % indexSize;
            var it = tailIndex[ithc];
            it.splice(it.indexOf(this), 1);
            this.tail.refCount--;
            if (this.tail.isLoose()) this.tail.disconnect();
        }

        loose.push(this);
    },

    /**
     * Roots an arrow
     * @this {Arrow}
     */
    root: function () {
        if (this.rooted) return;
        if (this.isLoose()) {
            this.connect();
        }
        this.rooted = true;
        changelog.push(this);
    },

    /**
     * Unroots an arrow
     * @this {Arrow}
     */
    unroot: function () {
        if (!this.rooted) return;
        this.rooted = false;
        if (this.isLoose()) {
            this.disconnect();
        }
        changelog.push(this);
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
    getChildren: function () {
        var children = [];

        // incoming
        var i = headIndex[this.hc % indexSize];
        if (i) Array.push.apply(children, i);

        // outgoing
        i = tailIndex[this.hc % indexSize];
        if (i) Array.push.apply(children, i);

        delete i; // prevent polluting the closure
        var indice = 0;
        var parent = this;
        return function () {
            // returns a real child, looping through false positives
            while (indice < children.length) {
                var a = children[indice++];
                if (a.getTail() == parent || a.getHead() == parent) return a;
            }
            return null;
        };
    }
});

/**
 * Creates an instance of Atom.
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
        loose.splice(loose.indexOf(this), 1);
    },

    /**
     * disconnect an atom
     * @this {Atom}
     */
    disconnect: function () {
        loose.push(this);
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
    }
});



/**
 * Creates an instance of Pair.
 * @class a pair is an non-atomic arrow
 * @constructor
 * @this {Pair}
 * @extends {Arrow}
 * @param {Arrow} t tail
 * @param {Arrow} h head
 * @param {number} hc hashcode
 */
function Pair(t, h, hc) {
    if (hc === undefined) hc = (String(t.hc) + String(h.hc)).hashCode();
    Arrow.call(t, h, hc);
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
 * get the unique arrow corresponding to a piece of data
 * @param {boolean} test existency test only => no creation
 */
Arrow.atom = function (body, test) {
    var atom;
    var hc = String(body).hashCode();
    var a = hc % memSize;
    var i = 0;
    var l = defIndex[a];
    if (!l) l = defIndex[a] = [];

    for (i = 0; i < length && atom = l[i] && !(atom.hc == hc && atom.body == body); i++);
    if (i < l.length) return atom;

    if (test) return undefined;
    
    l.push(atom = new Atom(body, hc));
    return atom;
};

/**
 * get the unique arrow linking t (its tail) to h (its head)
 * @param {Arrow} t the tail
 * @param {Arrow} h the head
 * @param {boolean} test existency test only => no creation
 */
Arrow.pair = function (t, h, test) {
    var pair;
    var hc = (String(t.hc) + String(h.hc)).hashCode();
    var a = hc;
    var l = defIndex[a];
    var i = 0;
    if (!l) l = defIndex[a] = [];
    
    for (i = 0; i < l.length && pair = l[i] && !(pair.hc == hc && pair.head === h && pair.tail === t); i++);
    if (i < l.length) return pair;
    
    if (test) return undefined;
    
    l.push(pair = new Pair(h, t, hc));
    return pair;
};

/**
 * get an arrow placeholder
 * param {uri} uri that we haven't opened yet
 * @return {Arrow}
 */
Arrow.placeholder = function (uri) {
    var hc = String(uri).hashCode();

    return new Arrow(undefined, undefined, hc);
}

/**
 * Garbage collector
 */
Arrow.gc = function () {
    if (changelog.length) return;

    for (var i = 0; i < loose.length; i++) {
        var lost = loose[i];
        var a = lost.hc;
        var l = defIndex[a];
        l.splice(l.indexOf(lost), 1);
    }
    loose.splice(0, loose.length);
};



/**
 * Local changes commit
 */
Arrow.commit = function (secondTry) {
    var promise = $.when({
        ready: true
    });

    for (var i = 0; i < changelog.length; i++) {
        var changed = changelog[i];
        if (changed.isRooted()) {
            promise = promise.pipe(function () {
                return ground.root(changed);
            });
        } else {
            promise = promise.pipe(function () {
                return ground.unroot(changed);
            });
        }
    }
    promise = promise.pipe(function () {
        changelog.splice(0, changelog.length);
        return ground.commit();
    }).done(function () {
        Arrow.gc();
    });

    if (!secondTry) {
        // second try in case of failure
        promise.pipe(nul, function () {
            ground.reset();
            return Arrow.commit(true);
        });
    }
    return promise;
};



/**
 * the ground object is a proxy beside a public entrelacs server
 * where arrows are really saved when rooted
 */
var ground = {
    /** map URI <-> local arrow */
    uriMap: {},

    /** server base URL */
    serverUrl: "http://miellaby.selfip.net:8008",

    /** last known cookie, */
    lastCookie: null,

    /**
     * reset session
     * @return {boolean} session was empty while reseting
     */
    reset: function () {
        var wasEmpty = true;
        for (var uri in this.uriMap) {
            delete this.uriMap[uri].uri;
            wasEmpty = false;
        }
        this.uriMap = {};
        return wasEmpty;
    },

    /** 
     * check cookie and detect session loss
     */
    checkCookie: function () {
        var currentCookie = $.cookie('session');
        if (currentCookie != this.lastCookie) {
            // session is lost, invalidate all URI
            this.lastCookie = currentCookie;
            this.reset();
        }
    },

    /**
     * bind an URI to an arrow. Called by ground.getUri or ground.open
     * @param {Arrow}
     */
    bindUri: function (a, uri) {
        var p = this.uriMap[uri]; // is there a placeholder here?
        if (p) {
            // p == a, let's rewire all its children
            var child, iterator = p.getChildren();
            while ((child = iterator()) != null) {
                child.disconnect();
                if (child.head === p) {
                    child.head = a;
                }
                if (child.tail === p) {
                    child.tail = a;
                }
                child.connect(); // easy as pie
            }
        }

        a.uri = uri;
        this.uriMap[uri] = a;
        // at this step, p should be GCed by JS
    },

    /**
     * get an URI for an arrow
     * @param {Arrow}
     * @return {Promise}
     */
    getUri: function (a) {
        var promise;

        if (a.uri) {
            return $.when(a.uri);
        }

        if (a.isAtomic()) {
            var req = this.serverUrl + '/escape+' +
                encodeURIComponent(a.getBody()) + '?iDepth=0';
            promise = $.get(req);

        } else {
            var pt = ground.getUri(a.getTail());
            var ph = ground.getUri(a.getHead());
            promise = $.when(pt, ph).pipe(function () {
                var req = this.serverUrl + '/escape/'
                    + a.getTail().uri + '+' + a.getHead().uri + '?iDepth=0';
                return $.get(req);
            });
        }

        promise.done(function (uri) {
            ground.checkCookie();
            ground.bindUri(a, uri);
        }).fail(function () {
            ground.reset();
        });

        return promise;
    },

    /**
     * root an arrow
     * @param {Arrow}
     * @return {Promise}
     */
    root: function (a) {
        var promise = this.getUri(a);
        promise = promise.pipe(function (uri) {
            var req = this.serverUrl + '/root/escape+' + uri;
            var promise = $.get(req);
            return promise;
        }).fail(function () {
            ground.reset();
        });
        return promise;
    },

    /**
     * unroot an arrow
     * @param {Arrow}
     * @return {Promise}
     */
    unroot: function (a) {
        var promise = this.getUri(a);
        promise = promise.pipe(function (uri) {
            var req = this.serverUrl + '/unroot/escape+' + uri;
            var promise = $.get(req);
            return promise;
        }).fail(function () {
            ground.reset();
        });
        return promise;
    },

    /**
     * isRooted
     * @param {Arrow}
     * @return {Promise}
     */
    isRooted: function (a) {
        var promise = this.getUri(a);
        promise = promise.pipe(function (uri) {
            var req = this.serverUrl + '/isRooted/escape+' + uri;
            var promise = $.get(req, function (r) {
                r && a.root() || a.unroot();
            });
            return promise;
        }).fail(function () {
            ground.reset();
        });
        return promise;
    },

    /**
     * getChildren
     * @param {Arrow}
     * @return {Arrow}
     */
    getChildren: function (a) {
        var promise = this.getUri(a);
        promise = promise.pipe(function (uri) {
            var req = this.serverUrl + '/childrenOf/escape+' + uri;
            var promise = $.get(req);
            return promise;
        }).fail(function () {
            ground.reset();
        }).done(function (arrowURI) {
            a.gchildren = ground.parse(arrowURI);
        });
        return promise;
    },

    /**
     * invoke an arrow (evaluate)
     * @param {Arrow}
     * @return {Promise}
     */
    invoke: function (a) {
        var promise = this.getUri(a);
        promise = promise.pipe(function (uri) {
            var req = this.serverUrl + '/' + uri; // no escape this time
            var promise = $.get(req);
            return promise;
        }).fail(function () {
            ground.reset();
        });
        return promise;
    },

    /**
     * open/unfold/explore an arrow placeholder
     * @param {Arrow} a placeholder
     * @return {Promise}
     */
    open: function (p, depth) {
        depth = depth || 5;
        var uri = p.uri;
        var req = this.serverUrl + '/escape' + uri + '?iDepth=' + depth;

        var promise = $.get(req, function (uri) {
            var unfolded = ground.parseURI(uri);
            ground.bindUri(unfolded, p.uri);
        });

        return promise;
    }

    /**
     * look at a local arrow with the given uri
     * @param {string} uri
     * @return {Arrow} the actual arrow or a `placeholder' arrow
     */
    fromURI: function (uri) {
        var a = this.uriMap[uri];
        if (a) return a;
        a = Arrow.placeholder(uri);
        a.uri = uri;
        this.uriMap[uri] = a;
        return a;
    },
    /**
     * commit
     * @return {Promise}
     */
    commit: function () {
        var req = this.serverUrl + '/commit+';
        var promise = $.get(req).fail(function () {
            ground.reset();
        });
        return promise;
    }
};


// Simple recursive descent parser for Entrelacs URI
ground.parseURI = function () {
    // We define it inside a closure to keep bits private.

    var at, // The index of the current character
    ch, // The current character
    text, // the text being parsed

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
        var hex, i, ff, string = '';
        if (ch === '$') {
            // stop at + or / characters.
            while (ch !== '+' && ch !== '/' && ch != '') {
                string += ch;
                next();
            }
            return ground.fromURI(string);
        }
    },

    atom = function () { // Parse an atom.
        var hex, i, ff, string = '';

        // stop at + or / characters.
        while (ch !== '+' && ch !== '/' && ch != '') {
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
        return Arrow.atom(string);
    },

    arrowURI, // Place holder for the value function.

    pair = function () { // Parse a pair.
        var head, tail;
        if (ch === '/') {
            next();
            head = arrow();
            if (ch === '+') next();
            tail = arrow();
            return Arrow.pair(tail, head);
        }
    };

    arrowURI = function () { // Parse a pair or an atom
        return (ch === '/' ? pair() : (ch === '$' ? ref() : atom()));
    };

    // Return the overall parsing function
    return function (source) {
        text = source;
        at = 0;
        next();
        return arrowURI();
    };
}();
