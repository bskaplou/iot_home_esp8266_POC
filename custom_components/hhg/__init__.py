"""The example sensor integration."""
DOMAIN = 'hhg'


def setup(hass, config):
    """Your controller/hub specific code."""
    # Data that you want to share with your platforms
    #hass.data[DOMAIN] = {
    #    'temperature': 23
    #}

    hass.helpers.discovery.load_platform('sensor', DOMAIN, {}, config)

    return True
