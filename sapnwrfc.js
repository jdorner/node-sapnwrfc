var majMinVersion = process.versions.node.match(/^[0-9]+.[0-9]+/)[0] || '';
var sapnwrfc = require('bindings')({bindings: 'sapnwrfc', version: majMinVersion});

var _logger;

sapnwrfc.configure = function(options) {
    if(options) {
        _logger = options.logger;
    }
}

function _log(level, message, meta) {
    if(_logger) {
        if(meta) {
            _logger.log(level, message, meta);
        } else {
            _logger.log(level, message);
        }
    }
};

sapnwrfc.Connection.prototype._log = _log;
sapnwrfc.Function.prototype._log = _log;

module.exports = sapnwrfc;
