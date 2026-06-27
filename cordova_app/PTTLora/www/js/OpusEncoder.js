/**
 * OpusEncoder - JavaScript interface to Android MediaCodec Opus codec
 */
var OpusEncoder = {
    encoderInitialized: false,

    init: function(callback) {
        cordova.exec(function() {
            this.encoderInitialized = true;
            if (callback) callback();
        }.bind(this), 
        function(err) { console.error('OpusEncoder init failed: ' + err); },
        'OpusEncoder', 'initEncoder', [8000, 1, 16000]);
    },

    encodeFrame: function(pcmBytes) {
        if (!this.encoderInitialized) return Promise.resolve(null);
        return new Promise(function(resolve, reject) {
            cordova.exec(function(result) { resolve(result[0]); }, 
                          reject, 'OpusEncoder', 'encodeFrame', [pcmBytes]);
        });
    },

    playPCM: function(opusBytes) {
        return new Promise(function(resolve, reject) {
            cordova.exec(function() { resolve(); }, reject, 'OpusEncoder', 'playPCM', [opusBytes]);
        });
    },

    releaseAll: function() {
        cordova.exec(null, null, 'OpusEncoder', 'releaseAll', []);
        this.encoderInitialized = false;
    }
};
