var majMinVersion = process.versions.node.match(/^[0-9]+.[0-9]+/)[0] || '';
module.exports = require('bindings')({ bindings: 'sapnwrfc', version: majMinVersion });