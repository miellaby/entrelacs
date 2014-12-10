// global variables
var entrelacs, terminal;

function init() {
    var area = $('#area');
    entrelacs = new Entrelacs();
    terminal = new Terminal(area, entrelacs, true);

    //findFeaturedArrows();
    var wizardState = "beginning";
    
    if ($.cookie && $.cookie('wizard') == '1'
        || window.location.hash) {
        return;
    }
    
    var center = function(elt) {
        var p = elt.position();
        var x = p.left - ($(window).width() / 2);
        var y = p.top - elt.height() - ($(window).height() * 0.5);
        $('.wizard_box').delay(1500).fadeIn();

        terminal.scroll(-x, -y);
    };

    $('.wizard_box').delay(1500).fadeIn();

    terminal.entrelacs.isRooted(Arrow.pair(Arrow.atom('hello'), Arrow.atom('world'))).done(function(rooted) {
        if (rooted) {
            wizardState = 'clean';
        }
    });

    var wizardScrutation = setInterval(function() {
        var o;
        if (wizardState == "beginning") {
            if ((o = $(':focus').parent()).hasClass('prompt')) {
                $('#wizard_says').hide().html("Type <i>hello</i><br/>and validate.").fadeIn();
                center(o);
                wizardState = "hello";
            }
        } else if (wizardState == 'hello') {
            if ((o = Arrow.atom('hello').get('views')) !== undefined) {
                $('#wizard_says').hide().html("You've submited an <i>atomic arrow</i>.<p>Click the &darr;&nbsp;button in its attached toolbar.").fadeIn();
                center(o[0].d);
                wizardState = "outgoing";
            }
        } else if (wizardState == 'outgoing') {
            if ($(':focus').parent().hasClass('prompt')) {
                var f = $(':focus').parent().data('view').for;
                if (f && f.length && f[0].tv.arrow == Arrow.atom('hello')) {
                    $('#wizard_says').hide().html("You are now defining another arrow.<p> Enter <i>world</i> at the other end").fadeIn();
                    center(f[0].d);
                    wizardState = "world";
                }
            }
        } else if (wizardState == 'world') {
            var a = Arrow.pair(Arrow.atom('hello'), Arrow.atom('world'))
            if (a.isRooted()) {
                $('#wizard_says').hide().html("Here is a regular arrow. Every arrow is <i>unique</i> and <i>self-indexed</i>. This one has been <i>rooted</i> into the system.<p>Please <a href='#' onClick='document.location.reload();'>refresh</a> the page now!").fadeIn();
                center(a.get('views')[0].d);
                wizardState = "clean";
            }
        } else if (wizardState == 'clean') {
            if (!(Arrow.atom('hello').get('views') || []).length && !((Arrow.pair(Arrow.atom('hello'), Arrow.atom('world')).get('views') || []).length)) {
                $('#wizard_says').hide().html("Let's check how the system keeps rooted arrows. <p>Submit <i>hello</i> again.").fadeIn();
                wizardState = 'hello2';
            }
        } else if (wizardState == 'hello2') {
            if ((o = Arrow.atom('hello').get('views') || []).length) {
                $('#wizard_says').hide()
                  .html("Arrows are connected to each others."
                    + " From an arrow, one may browse"
                    + " <i>incoming</i> and <i>outgoing</i> children."
                    + " Keep on experimenting!"
                    + " You may submit media, walls of text, and build"
                    + " complex arrows that you may browse back by any end.").fadeIn();
                center(o[0].d);
                wizardState = 'end';
                $.cookie('wizard', '1');
                clearInterval(wizardScrutation);
                $('.wizard_box').delay(10000).fadeOut(4000);
            }
        }
       
    }, 1000);

    $('.wizard_box>.close>a').click(function() {
        $('.wizard_box>.close').hide();
        $('#wizard_says').hide().html("Erase cookies to make me come again!").fadeIn();
        wizardState = 'end';
        $.cookie('wizard', '1');
        clearInterval(wizardScrutation);
        $('.wizard_box').fadeOut(4000);
        return false;
    });
    
}

$(document).ready(init);