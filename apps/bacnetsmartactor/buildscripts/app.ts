import * as forge from 'node-forge'
import * as fs from 'fs';
const password = "password";


const encryptedPrivateKeyFile = "./encryptedPrivateKey.pem";

const pki=forge.pki;
const rsa=pki.rsa;
const sha256 = forge.md.sha256;

let encryptedPrivateKeyPem:string;
if(!fs.existsSync(encryptedPrivateKeyFile))
{
    console.log('Generating random new 2048-bit key-pair...');
    const keypair = rsa.generateKeyPair(2048);
    encryptedPrivateKeyPem = pki.encryptRsaPrivateKey(keypair.privateKey, password);
    fs.writeFileSync(encryptedPrivateKeyFile, encryptedPrivateKeyPem);
}
else{
    encryptedPrivateKeyPem=fs.readFileSync(encryptedPrivateKeyFile).toString();
}


const rootCertAuthorityAttr = [
    { name: 'commonName', value: 'Liebler Certificate Authortity' },
    { name: 'countryName', value: 'DE' },
    { name: 'localityName', value: 'Gelsenkirchen' },
    { name: 'organizationName', value: 'Liebler Certificate Authortity Organization' },
    { shortName: 'OU', value: 'Root Certificate Authority' },
    { shortName: 'ST', value: 'North Rhine Westfalia' }
  ];
  
  const certAuthorityExtensions = [
      {
          name: 'basicConstraints',
          cA: true,
      }, {
          name: 'keyUsage',
          keyCertSign: true,
          cRLSign: true,
      }
  ];
  
  const caKeyPair = rsa.generateKeyPair(2048);
  const caCert = pki.createCertificate();
  caCert.publicKey = caKeyPair.publicKey;
  caCert.serialNumber = randomSerialNumberAsString();
  caCert.validity.notBefore = new Date();
  caCert.validity.notAfter = new Date();
  caCert.validity.notAfter.setFullYear(caCert.validity.notBefore.getFullYear() + 1);
  
  caCert.setSubject(rootCertAuthorityAttr);
  caCert.setIssuer(rootCertAuthorityAttr);
  caCert.setExtensions(certAuthorityExtensions);
  caCert.sign(caKeyPair.privateKey, sha256.create())
  
  const caCertKeyPem = pki.encryptRsaPrivateKey(caKeyPair.privateKey, 'password');
  const caCertPem = pki.certificateToPem(caCert);

  const intermediateCertAuthorityAttr = [
    { name: 'commonName', value: 'Liebler Intermediate Certificate Authority' },
    { name: 'countryName', value: 'DE' },
    { name: 'localityName', value: 'Gelsenkirchen' },
    { name: 'organizationName', value: 'example organization' },
    { shortName: 'OU', value: 'Intermediate Certificate Authority' },
    { shortName: 'ST', value: 'North Rhine Westfalia' }
  ];
  
  const intCaKeyPair = rsa.generateKeyPair(2048);
  const intCaCert = pki.createCertificate();
  intCaCert.publicKey = intCaKeyPair.publicKey
  intCaCert.serialNumber = randomSerialNumberAsString()
  intCaCert.validity.notBefore = new Date()
  intCaCert.validity.notAfter = new Date()
  intCaCert.validity.notAfter.setFullYear(intCaCert.validity.notBefore.getFullYear() + 1)
  
  intCaCert.setSubject(intermediateCertAuthorityAttr)
  intCaCert.setIssuer(rootCertAuthorityAttr)
  intCaCert.setExtensions(certAuthorityExtensions)
  intCaCert.sign(caKeyPair.privateKey, sha256.create())
  
  const intCaCertKeyPem = pki.encryptRsaPrivateKey(caKeyPair.privateKey, 'password for interm CA')
  const intCaCertPem = pki.certificateToPem(intCaCert)


  const serverAttr = [
	{ name: 'commonName', value: '*.myServer.test' },
    { name: 'countryName', value: 'US' },
  	{ name: 'localityName', value: 'Kirkland' },
  	{ name: 'organizationName', value: 'example organization' },
  	{ shortName: 'OU', value: 'myServer' },
  	{ shortName: 'ST', value: 'Washington' }
]
const serverExtensions = [
    // Authority Key Identifier?
    {
        name: 'keyUsage',
        digitalSignature: true,
		keyEncipherment: true
    },
	{
		name: 'extKeyUsage',
		serverAuth: true,
	}, 
    {
	    name: 'subjectAltName',        
        altNames: [{
            type: 2, // dNSName
            value: '*.myServer.test'
        }, {
            type: 7, // IP
            ip: '127.0.0.1'
        }]
    }, {
 		name: 'subjectKeyIdentifier'
	}
]


const serverKeyPair = rsa.generateKeyPair(2048)
const serverCert = pki.createCertificate()
serverCert.publicKey = serverKeyPair.publicKey
serverCert.serialNumber = randomSerialNumberAsString()
serverCert.validity.notBefore = new Date()
serverCert.validity.notAfter = new Date()
serverCert.validity.notAfter.setFullYear(serverCert.validity.notBefore.getFullYear() + 1)

serverCert.setSubject(serverAttr)
serverCert.setIssuer(intermediateCertAuthorityAttr)
serverCert.setExtensions(serverExtensions)

const signingKey = pki.decryptRsaPrivateKey(intCaCertKeyPem, 'password')
      
serverCert.sign(signingKey, sha256.create())

const serverKeyPem = pki.privateKeyToPem(serverKeyPair.privateKey)
const serverCertPem = pki.certificateToPem(intCaCert)


  // a hexString is considered negative if it's most significant bit is 1
// because serial numbers use ones' complement notation
// this RFC in section 4.1.2.2 requires serial numbers to be positive
// http://www.ietf.org/rfc/rfc5280.txt
function toPositiveHex(hexString:string):string{
    var mostSiginficativeHexAsInt = parseInt(hexString[0], 16);
    if (mostSiginficativeHexAsInt < 8){
      return hexString;
    }
  
    mostSiginficativeHexAsInt -= 8;
    return mostSiginficativeHexAsInt.toString() + hexString.substring(1);
  }
  
  function randomSerialNumberAsString ():string {
      return toPositiveHex(forge.util.bytesToHex(forge.random.getBytesSync(9)))
  }