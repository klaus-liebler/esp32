"use strict";
exports.__esModule = true;
var forge = require("node-forge");
var fs = require("fs");
var password = "password";
console.log('Generating random new 2048-bit key-pair...');
var keypair = forge.pki.rsa.generateKeyPair(/* bits */ 2048 /*, e = 0x1001 */);
var encryptedPrivateKeyPem = forge.pki.encryptRsaPrivateKey(keypair.privateKey, password);
fs.writeFileSync("./encryptedPrivateKey.pem", encryptedPrivateKeyPem);
