#!/usr/bin/env node
console.log("__dirname : " + __dirname)
global.__rootDirectory = __dirname + '/node_modules/pyright/dist/';
require('./node_modules/pyright/dist/pyright');
