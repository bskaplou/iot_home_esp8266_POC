const bonjour = require("bonjour")()
const http = require("http")

let discovery_callbacks = []
let discovery_found = []

const browser = bonjour.find({}, function (service) {
  console.log(service)
  discovery_deliver()
})

browser.start()

async function pereodic_broadcasts() {
  while(true) {
    await new Promise(r => setTimeout(r, 5000))
    console.log("broadcast")
    browser.update()
  }
}

new Promise(()=>pereodic_broadcasts())

function discovery_deliver() {
  if (discovery_callbacks.length > 0 && discovery_found.length > 0) {
    for (callback of discovery_callbacks) {
      callback(discovery_found)
    }
  }
}

exports.find = function (callback) {
  discovery_callbacks.push(callback)
  discovery_deliver()
}
