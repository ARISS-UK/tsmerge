/* BATC Player JS */
/* Requires jquery and clappr */
var player;

$(function() {
    
    $("#stream-stats-status").html("Standing by for HamTV Acquisition..");
    createPlayer();
});


function createPlayer()
{
    player = new Clappr.Player({
         source: rtmpUrl,
         plugins: {'playback': [RTMP]},
         playbackNotSupportedMessage: 'Video stream playback not supported.<br>Please install Adobe Flash Player or try a different browser.',
         rtmpConfig: {
             swfPath: 'libs/clappr/rtmp/assets/RTMP.swf',
             playbackType: 'live',
             scaling:'stretch',
             //bufferTime: 1,
             //startLevel: 0,
            width: '100%',
            height: '100%',
         },
         autoPlay: true,
     });
     $("#stream-player").height("360");
     $("#stream-player").width("640");
     
     var playerElement = document.getElementById("stream-player");
    player.attachTo(playerElement);
     
     $("#stream-stats").width("634");
}
