
function init() {
    var area = $('#area');
    var entrelacs = new Entrelacs();
    var terminal = new Terminal(area, entrelacs, true);
    $(document).scrollTop(area.height() / 2 - $(window).height() / 2);
    $(document).scrollLeft(area.width() / 2 - $(window).width() / 2);
    setTimeout(function() { $("#killme").fadeOut(4000, function(){$(this).detach();}); }, 200);

    
    creole = new Parse.Simple.Creole({forIE: document.all,
                                     interwiki: {
                WikiCreole: 'http://www.wikicreole.org/wiki/',
                Wikipedia: 'http://en.wikipedia.org/wiki/'},
                                     linkFormat: '' });
                                     
    //findFeaturedArrows();
    var wizardState = "beginning";
    if ($.cookie('wizard') == '1') return;
    
    var center = function(elt) {
           var p = elt.position();
            console.log(p);
           var x = p.left + elt.width() / 2 - ($(window).width() / 2);
           var y = p.top + elt.height() / 2 - ($(window).height() / 2) - $('.wizard_box').height() * 0.9;
           $('html, body').animate({scrollLeft: x, scrollTop: y}, 1000);
 
    };
    
    $('.wizard_box').delay(1500).fadeIn();
    var p = 
    terminal.entrelacs.isRooted(Arrow.pair(Arrow.atom('hello'), Arrow.atom('world'))).done(function(rooted) {
        if (rooted) {
            wizardState = 'clean';
        }
    });
    var wizardScrutation = setInterval(function() {
        var o;
        if (wizardState == "beginning") {
            if ((o = $(':focus').parent()).hasClass('prompt')) {
                $('#wizard_says').hide().html("Now please type <i>hello</i> and validate with the <i>Return/Enter</i> key").fadeIn();
                center(o);
                wizardState = "hello";
            }
        } else if (wizardState == 'hello') {
            if ((o = Arrow.atom('hello').get('views')) !== undefined) {
                $('#wizard_says').hide().html("You've just submitted your first <i>atomic arrow</i>.<p>Now find and click the &darr;&nbsp;button.").fadeIn();
                center($(o[0]));
                wizardState = "outgoing";
            }
        } else if (wizardState == 'outgoing') {
            if ($(':focus').parent().hasClass('prompt')) {
                var f = $(':focus').parent().data('for');
                if (f && f.length && f[0].data('tail').data('arrow') == Arrow.atom('hello')) {
                    $('#wizard_says').hide().html("You're bound to create another arrow.<p> Enter <i>world</i> at the other end").fadeIn();
                    center(f[0]);
                    wizardState = "world";
                }
            }
        } else if (wizardState == 'world') {
            if ((o = Arrow.pair(Arrow.atom('hello'), Arrow.atom('world')).get('views')) !== undefined) {
                $('#wizard_says').hide().html("I introduce you the <i>hello</i> to <i>world</i> arrow. It is <i>unique</i> and <i>self-indexed</i>, like every other arrow.<p> Now please check the box in its floating toolbar").fadeIn();
                center($(o[0]));
                wizardState = "root";
            }

        } else if (wizardState == 'root') {
            if ((o = Arrow.pair(Arrow.atom('hello'), Arrow.atom('world'))).isRooted()) {
                $('#wizard_says').hide().html("So you've just <i>rooted hello&rarr;word</i>&nbsp!<br>:)<br>That tells the system this arrow is right and worth keeping.<p>Would you mind refresh the page now?").fadeIn();
                center($(o.get('views')[0]));
                wizardState = "clean";
            }
        } else if (wizardState == 'clean') {
            if (!(Arrow.atom('hello').get('views') || []).length && !((Arrow.pair(Arrow.atom('hello'), Arrow.atom('world')).get('views') || []).length)) {
                $('#wizard_says').hide().html("Welcome back.<p>Submit <i>hello</i> again and we're done").fadeIn();
                wizardState = 'hello2';
            }
        } else if (wizardState == 'hello2') {
            if ((o = Arrow.atom('hello').get('views') || []).length) {
                $('#wizard_says').hide().html("Note how arrows are connected to each others, "
                    + "allowing information to be browsed as will!<p>The tutorial is over but keep on experimenting!").fadeIn();
                center($(o[0]));
                wizardState = 'end';
                $.cookie('wizard', '1');
                clearInterval(wizardScrutation);
                $('.wizard_box').delay(5000).fadeOut(4000);
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