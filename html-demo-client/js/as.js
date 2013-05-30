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
 * Creates an instance of {Arrow}.
 * @class {Arrow} 'Everything is an arrow'. That is an oriented pair of {Arrow}s.
 * @constructor
 * @this {Arrow}
 * @param {Arrow} t tail
 * @param {Arrow} h head
 * @param {number} hc The hashcode for this arrow.
 */
function Arrow(t, h, hc) {
    this.tail = t;
    this.head = h;
    this.hc = hc;
    this.refCount = 0;
    this.rooted = false;
    Arrow.loose.push(this);
}

// Some constants
Arrow.indexSize = 2000; // "Nobody Will Ever Need More Than 640k RAM!"
Arrow.publicEntrelacsServer = "http://miellaby.selfip.net:8008/"; // Famous well-known epic server

// indexes (as maps of lists of arrows)
//
//      Please note that implementations at lower levels merge
//    arrow storage and indexes in a single memory space.
//
Arrow.defIndex = new Array(Arrow.indexSize);
Arrow.headIndex = new Array(Arrow.indexSize);
Arrow.tailIndex = new Array(Arrow.indexSize);

// changes log
Arrow.changelog = [];

// loose arrows
Arrow.loose = [];

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
            this.head.refCount++;
            if (this.head.isLoose()) this.head.connect();
        }

        // to tail
        if (this.tail) {
            // ithc: hash code for index
            var ithc = this.tail.hc % Arrow.indexSize;
            var it = Arrow.tailIndex[ithc];
            if (!it) it = Arrow.tailIndex[ithc] = [];
            it.push(this);
            this.tail.refCount++;
            if (this.tail.isLoose()) this.tail.connect();
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
        if (this.rooted) return;
        if (this.isLoose()) {
            this.connect();
        }
        this.rooted = true;
        Arrow.changelog.push(this);
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
        Arrow.changelog.push(this);
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
        var i = Arrow.headIndex[this.hc % Arrow.indexSize];
        if (i) Array.prototype.push.apply(children, i);

        // outgoing
        i = Arrow.tailIndex[this.hc % Arrow.indexSize];
        if (i) Array.prototype.push.apply(children, i);

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
function Placeholder(url, hc) {
    if (hc === undefined) hc = url.hashCode();
    Arrow.call(this, undefined, undefined, hc);
    this.url = url;
}

$.extend(Placeholder.prototype, Arrow.prototype);



/**
* Forget everything
*/
Arrow.wipe = function () {
  Arrow.defIndex = [];
  Arrow.headIndex = [];
  Arrow.tailIndex = [];
  Arrow.changelog = [];
  Arrow.loose = [];
}


/**
 * Finds o builds the unique atomic arrow corresponding to a piece of data
 * @param {boolean} test existency test only => no creation
 */
Arrow.atom = function (body, test) {
    var atom;
    var hc = String(body).hashCode();
    var a = hc % Arrow.indexSize;
    var i = 0;
    var l = Arrow.defIndex[a];
    if (!l) l = Arrow.defIndex[a] = [];

    for (i = 0; i < length && (atom = l[i]) && !(atom.hc == hc && atom.body == body); i++);
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
 * param {url} url that we haven't yet resolved to a local arrow
 * @return {Arrow}
 */
Arrow.placeholder = function (url) {
    var placeholder;
    var hc = String(url).hashCode();
    var a = hc % Arrow.indexSize;
    var l = Arrow.defIndex[a];
    var i = 0;

    if (!l) l = Arrow.defIndex[a] = [];    
    for (i = 0; i < l.length && (placeholder = l[i]) && !(placeholder.url == url); i++);
    if (i < l.length) return placeholder;

    placeholder = new Placeholder(url, hc);
    return placeholder;
}


/**
 * Parses an URI to find or build its corresponding unique arrow
 * @param {string} Entrelacs-Compatible URI
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
            return Arrow.placeholder(baseUrl + string);
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

    arrow, // Place holder for the value function.

    pair = function () { // Parse a pair.
        var tail, head;
        if (ch === '/') {
            next();
            tail = arrow();
            if (ch === '+') next();
            head = arrow();
            return Arrow.pair(tail, head);
        }
    };

    arrow = function () { // Parse a pair or an atom
        return (ch === '/' ? pair() : (ch === '$' ? ref() : atom()));
    };

    // Return the overall parsing function
    return function (uri, url) {
        baseUrl = url || Arrow.publicEntrelacsServer;
        text = uri;
        at = 0;
        next();
        return arrow();
    };
}();


/**
 * Garbage collector
 */
Arrow.gc = function () {
    // on a lower-level system, the arrow GC can't run between 2 commits as it may free unrooted arrows and its descendants before the changes have been commited
    // if (Arrow.changelog.length) return;

    for (var i = 0; i < Arrow.loose.length; i++) {
        var lost = Arrow.loose[i];
        var a = lost.hc % Arrow.indexSize;
        var l = Arrow.defIndex[a];
        l.splice(l.indexOf(lost), 1);
    }
    Arrow.loose.splice(0, Arrow.loose.length);
};



/**
 * Local changes commit
 ** Note: it might be perfectly feasible to commit on several servers at once
 */
Arrow.commit = function (entrelacs, secondTry) {
    var promise = $.when({
        ready: true
    });

    for (var i = 0; i < Arrow.changelog.length; i++) {
        var changed = Arrow.changelog[i];
        if (changed.isRooted()) {
            promise = promise.pipe(function () {
                return entrelacs.root(changed);
            });
        } else {
            promise = promise.pipe(function () {
                return entrelacs.unroot(changed);
            });
        }
    }
    promise = promise.pipe(function () {
        return entrelacs.commit();
    }).done(function () {
        Arrow.changelog.splice(0, Arrow.changelog.length);
        Arrow.gc();
    });

    if (!secondTry) {
        // second try in case of failure
        promise.pipe(null, function () {
            entrelacs.reset();
            return Arrow.commit(entrelacs, true);
        });
    }
    return promise;
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

    /** map URI <-> local arrow */
    this.uriMap = {},

    /** last known cookie, */
    this.lastCookie = null;
    
    /** uri key use to attach remote temporary URI to local arrows */
    this.uriKey = 'uriOf<' + this.serverUrl + '>';
};

$.extend(Entrelacs.prototype, {

    /**
     * reset proxy session
     */
    reset: function () {
        for (var uri in this.uriMap) {
            delete this.uriMap[uri][this.uriKey];
        }
        this.uriMap = {};
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
     * bind an URI to an arrow. Called by self.getUri or self.open
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

        a[this.uriKey] = uri;
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
        var self = this;
        
        if (a[this.uriKey]) {
            return $.when(a[this.uriKey]);
        }

        if (a.isAtomic()) {
            var req = this.serverUrl + '/escape+' +
                encodeURIComponent(a.getBody()) + '?iDepth=0';
            promise = $.ajax({url: req, xhrFields: { withCredentials: true }});

        } else {
            var pt = self.getUri(a.getTail());
            var ph = self.getUri(a.getHead());
            promise = $.when(pt, ph).pipe(function () {
                var req = self.serverUrl + '/escape/'
                    + a.getTail()[self.uriKey] + '+' + a.getHead()[self.uriKey] + '?iDepth=0';
                return $.ajax({url: req, xhrFields: { withCredentials: true }});
            });
        }

        promise.done(function (uri) {
            self.checkCookie();
            self.bindUri(a, uri);
        }).fail(function () {
            self.reset();
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
        var self = this;

        promise = promise.pipe(function (uri) {
            var req = self.serverUrl + '/root/escape+' + uri;
            var promise = $.ajax({url: req, xhrFields: { withCredentials: true }});
            return promise;
        }).fail(function () {
            self.reset();
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
        var self = this;

        promise = promise.pipe(function (uri) {
            var req = self.serverUrl + '/unroot/escape+' + uri;
            var promise = $.ajax({url: req, xhrFields: { withCredentials: true }});
            return promise;
        }).fail(function () {
            self.reset();
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
        var self = this;

        promise = promise.pipe(function (uri) {
            var req = self.serverUrl + '/isRooted/escape+' + uri;
            var promise = $.ajax({url: req, xhrFields: { withCredentials: true }, sucess: function (r) {
                r && a.root() || a.unroot();
            }});
            return promise;
        }).fail(function () {
            self.reset();
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
        var self = this;

        promise = promise.pipe(function (uri) {
            var req = self.serverUrl + '/childrenOf/escape+' + uri;
            var promise = $.ajax({url: req, xhrFields: { withCredentials: true }});
            return promise;
        }).fail(function () {
            self.reset();
        }).done(function (arrowURI) {
            a.gchildren = Arrow.decodeURI(arrowURI);
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
        var self = this;

        promise = promise.pipe(function (uri) {
            var req = self.serverUrl + '/' + uri; // no escape this time
            var promise = $.ajax({url: req, xhrFields: { withCredentials: true }});
            return promise;
        }).fail(function () {
            self.reset();
        });
        return promise;
    },

    /**
     * open/unfold/explore an arrow placeholder
     * @param {Arrow} p placeholder
     * @return {Promise}
     */
    open: function (p, depth) {
        depth = depth || 5;
        var self = this;
        var url = p.url;
        var req = url + '?iDepth=' + depth;

        var promise = $.ajax({url: req, xhrFields: { withCredentials: true }, sucess: function (uri) {
            var unfolded = Arrow.decodeURI(uri);
            // rattach the URI to the unfolded arrow as we know it from the placeholder
            self.bindUri(unfolded, p[self.uriKey]);
        }});

        return promise;
    },

    /**
     * look at a local arrow with the given uri
     * @param {string} uri
     * @return {Arrow} the actual arrow or a `placeholder' arrow
     */
    fromURI: function (uri) {
        var a = this.uriMap[uri];
        if (a) return a;
        a = Arrow.placeholder(this.serverUrl + '/' + uri);
        a[this.uriKey] = uri;
        this.uriMap[uri] = a;
        return a;
    },
    
    
    /**
     * commit
     * @return {Promise}
     */
    commit: function () {
        var req = this.serverUrl + '/commit+';
        var promise = $.ajax({url: req, xhrFields: { withCredentials: true }}).fail(function () {
            self.reset();
        });
        return promise;
    }
});
