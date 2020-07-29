const crypto = require('crypto')


function random_salt() {
  return Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15)
}

const algo = 'aes-128-cbc'
const password = 'Password used to generate key'
const key = crypto.scryptSync(password, random_salt(), 16)
const iv = Buffer.alloc(16, 0)

const cipher = crypto.createCipheriv(algo, key, iv)
const decipher = crypto.createDecipheriv(algo, key, iv)

let encrypted = ''
cipher.on('readable', () => {
  let chunk
  while (null !== (chunk = cipher.read())) {
    encrypted += chunk.toString('hex')
  }
})
cipher.on('end', () => {
  console.log(encrypted)
})

cipher.write('some clear text data')
cipher.end()

let decrypted = ''
decipher.on('readable', () => {
  while (null !== (chunk = decipher.read())) {
    decrypted += chunk.toString('utf8')
  }
})
decipher.on('end', () => {
  console.log(decrypted)
})

decipher.write(encrypted, 'hex')
decipher.end()
