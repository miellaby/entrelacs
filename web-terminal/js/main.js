
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
    if ($.cookie('wizard') == 'end') return;
    $('.wizard_box').fadeIn();
    var wizardScrutation = setInterval(function() {
        if (wizardState == "beginning") {
            if ($(':focus').parent().hasClass('prompt')) {
                $('#wizard_says').hide().html("Now please type <i>hello</i> and validate with the <i>Return/Enter</i> key").fadeIn();
                wizardState = "hello";
            }
        } else if (wizardState == 'hello') {
            if (Arrow.atom('hello').get('views') !== undefined) {
                $('#wizard_says').hide().html("You've just submitted your first <i>atomic arrow</i>. Now use &darr; in the toolbar to start a second arrow from it.").fadeIn();
                wizardState = "outgoing";
            }
        } else if (wizardState == 'outgoing') {
            if ($(':focus').parent().hasClass('prompt')) {
                var f = $(':focus').parent().data('for');
                if (f && f.length && f[0].data('tail').data('arrow') == Arrow.atom('hello')) {
                    $('#wizard_says').hide().html(" Enter <i>world</i> as the other arrow end.").fadeIn();
                    wizardState = "world";
                }
            }
        } else if (wizardState == 'world') {
            if (Arrow.pair(Arrow.atom('hello'), Arrow.atom('world')).get('views') !== undefined) {
                $('#wizard_says').hide().html("May I introduce the <i>hello</i> to <i>world</i> arrow? Like every mathematicaly definable arrow, it is <i>unique</i> and <i>self-indexed</i>. Now please check the box in its floating toolbar.").fadeIn();
                wizardState = "root";
            }

        } else if (wizardState == 'root') {
            if (Arrow.pair(Arrow.atom('hello'), Arrow.atom('world')).isRooted()) {
                $('#wizard_says').hide().html("You've just <i>rooted</i> the 'hello word' arrow! :) It means you've set its one and only <i>mutable</i> property, telling the system it is 'true' and worth keeping.<br>Now erase all what you've drawn by clicking the &times buttons repeatidly.").fadeIn();
                wizardState = "clean";
            }
        } else if (wizardState == 'clean') {
            if (!Arrow.atom('hello').get('views').length && !Arrow.pair(Arrow.atom('hello'), Arrow.atom('world')).get('views').length) {
                $('#wizard_says').hide().html("OK. To finish this tutorial, please submit the <i>hello</i> atomic arrow again").fadeIn();
                wizardState = 'hello2';
            }
        } else if (wizardState == 'hello2') {
            if (Arrow.atom('hello').get('views').length) {
                $('#wizard_says').hide().html("That's all for now. Please note how arrows are connected to known relatives, "
                    + "allowing information to be browsed as will! Thanks for your time and keep on experimenting!").fadeIn();
                wizardState = 'end';
                $.cookie('wizard', 'end');
                setTimeout(function() {
                    clearInterval(wizardScrutation);
                    $('.wizard_box').fadeOut(4000);
                }, 5000);
            }
        }
       
    }, 1000);
    
}

$(document).ready(init);