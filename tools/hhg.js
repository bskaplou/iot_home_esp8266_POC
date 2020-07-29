const wifi = require("node-wifi")
const dgram = require('dgram')
const bonjour = require("bonjour")()
const {Signale} = require('signale')

// Too funny to stop
const signale = new Signale({
  disabled: false,
  interactive: false,
  logLevel: 'info',
  secrets: [],
  stream: process.stdout,
  types: {
    candidate_ssid: {
      badge: '☑',
      color: 'cyan',
      label: 'scan-result',
      logLevel: 'info'
    },
    other_ssid: {
      badge: '☐',
      color: 'gray',
      label: 'scan-result',
      logLevel: 'info'
    },
    unknown_ssid: {
      badge: '☒',
      color: 'red',
      label: 'scan-result',
      logLevel: 'info'
    },
    warning: {
      badge: '⚠',
      color: 'yellow',
      label: 'warning',
      logLevel: 'debug'
    },
    success: {
      badge: '✔',
      color: 'green',
      label: 'success',
      logLevel: 'debug'
    }
  }
})

function update_settings(ip, port, ssid, pwd, key) {
  return new Promise(async(success, error)=>{ 
    const socket = dgram.createSocket('udp4')
    socket.once('message', (message, client)=>{
      if(message == 'OK') {
        signale.success(`Message=>${message} from ${JSON.stringify(client)}`)
      } else {
        signale.error(`Message=>${message} from ${JSON.stringify(client)}`)
      }
      success(message)
    })
    await socket.bind(54321)
    const message = `return set_config("${ssid}", "${pwd}", "${key}")`
    const buff = Buffer.from(message, 'utf8')
    await socket.send(buff, 0, buff.length, port, ip)
  })
}

function is_candidate(wifi_net) {
  return wifi_net.ssid.startsWith('lonely_hhg_') && wifi_net.security == 'none'
}

async function scan_wifi() {
  signale.time("wifi-scan")
  const nets = await wifi.scan()
  signale.timeEnd("wifi-scan")
  return nets
}

async function search_wifi() {
  const candidates = []
  for(let net of await scan_wifi()) {
    if(is_candidate(net)) {
      candidates.push(net.ssid)
      signale.candidate_ssid(net.ssid)
    } else {
      signale.other_ssid(net.ssid)
    }
  }
  return candidates
}

function bonjour_find_one(filter, broadcast_delay = 10000) {
  return new Promise((success, error)=>{
    let found = false
    const browser = bonjour.find(filter, function (service) {
      found = true
      browser.stop()
      success(service)
    })
    async function pereodic_broadcasts() {
      while(! found) {
        signale.info(`broadcasting bonjour.find(${JSON.stringify(filter)})`)
        browser.update()
        await new Promise(r => setTimeout(r, broadcast_delay))
      }
    }
    pereodic_broadcasts()
  })
}

async function setup_ssid(ssid) {
  const nets = await scan_wifi()
  const match = nets.filter((net)=>net.ssid == ssid)

  if(match.length > 0) {
    const net = match[0]
    if(is_candidate(net)) {
      signale.success(`${net.ssid} found`)
      signale.time("connect")
      const initial_connections = await wifi.getCurrentConnections()
      await wifi.connect({ssid: net.ssid})
      signale.timeEnd("connect")

      const host_desc = await bonjour_find_one({type: 'hhg_udp'}, 500)
      signale.success(`${net.ssid} device discovered on address=>${host_desc.addresses}, port=>${host_desc.port}`)

      signale.time("update_settings")
      const result = await update_settings(host_desc.addresses[0], host_desc.port, "bskaplou@dybenko_2.4", "kerakera", "1234123412341232")
      signale.timeEnd("update_settings")

      signale.time("disconnect")
      try {
        await wifi.disconnect()
      } catch(e) {
        signale.warning(`please reconnect to ${initial_connections[0].ssid} manually`)
      }
      signale.timeEnd("disconnect")
      process.exit()
    } else {
      signale.unknown_ssid(`${net.ssid} is not compatible`)
    }
  } else {
    signale.unknown_ssid(`${ssid} not found`)
  }
}


wifi.init({
  iface: null // network interface, choose a random wifi interface if set to null
})

const yargs = require('yargs').argv
if(yargs['_'][0] == 'search') {
  new Promise(async()=>{
    const candidates = await search_wifi()
    if(candidates.length > 0) {
      signale.success(`Found candidate(s) for pairing ${candidates}`)
    } else {
      signale.error("No Wi-Fi access points ready for pairing found")
    }
    process.exit()
  })
} else if(yargs['_'][0] == 'setup_ssid'){
  new Promise(async ()=>{
    await setup_ssid(yargs['_'][1])
    process.exit()
  })
}
