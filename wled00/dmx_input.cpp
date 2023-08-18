#include "wled.h"

#ifdef WLED_ENABLE_DMX_INPUT

#ifdef ESP8266
#error DMX input is only supported on ESP32
#endif

#include "dmx_input.h"
#include <rdm/responder.h>

void rdmAddressChangedCb(dmx_port_t dmxPort, const rdm_header_t *header,
                         void *context)
{
  DMXInput *dmx = static_cast<DMXInput *>(context);

  if (!dmx)
  {
    USER_PRINTLN("DMX: Error: no context in rdmAddressChangedCb");
    return;
  }

  if (header->cc == RDM_CC_SET_COMMAND_RESPONSE)
  {
    const uint16_t addr = dmx_get_start_address(dmx->inputPortNum);
    DMXAddress = std::min(512, int(addr));
    USER_PRINTF("DMX start addr changed to: %d\n", DMXAddress);
  }
}

dmx_config_t DMXInput::createConfig() const
{

  dmx_config_t config;
  config.pd_size = 255;
  config.dmx_start_address = DMXAddress; // TODO split between input and output address
  config.model_id = 0;
  config.product_category = RDM_PRODUCT_CATEGORY_FIXTURE;
  config.software_version_id = VERSION;

  const std::string versionString = "WLED_V" + std::to_string(VERSION);
  strncpy(config.software_version_label, versionString.c_str(), 32);
  config.software_version_label[32] = '\0'; // zero termination in case versionString string was longer than 32 chars

  config.personalities[0].description = "SINGLE_RGB";
  config.personalities[0].footprint = 3;
  config.personalities[1].description = "SINGLE_DRGB";
  config.personalities[1].footprint = 4;
  config.personalities[2].description = "EFFECT";
  config.personalities[2].footprint = 15;
  config.personalities[3].description = "MULTIPLE_RGB";
  config.personalities[3].footprint = std::min(512, int(strip.getLengthTotal()) * 3);
  config.personalities[4].description = "MULTIPLE_DRGB";
  config.personalities[4].footprint = std::min(512, int(strip.getLengthTotal()) * 3 + 1);
  config.personalities[5].description = "MULTIPLE_RGBW";
  config.personalities[5].footprint = std::min(512, int(strip.getLengthTotal()) * 4);
  config.personalities[6].description = "EFFECT_W";
  config.personalities[6].footprint = 18;
  config.personalities[7].description = "EFFECT_SEGMENT";
  config.personalities[7].footprint = std::min(512, strip.getSegmentsNum() * 15);
  config.personalities[8].description = "EFFECT_SEGMENT_W";
  config.personalities[8].footprint = std::min(512, strip.getSegmentsNum() * 18);
  config.personalities[9].description = "PRESET";
  config.personalities[9].footprint = 1;

  config.personality_count = 10;
  // rdm personalities are numbered from 1, thus we can just set the DMXMode directly.
  config.current_personality = DMXMode;

  return config;
}

void DMXInput::init(uint8_t rxPin, uint8_t txPin, uint8_t enPin, uint8_t inputPortNum)
{

  if (inputPortNum < 3 && inputPortNum > 0)
  {
    this->inputPortNum = inputPortNum;
  }
  else
  {
    USER_PRINTF("DMXInput: Error: invalid inputPortNum: %d\n", inputPortNum);
    return;
  }

  /**
   * TODOS:
   * - attach callback for personality change and store in flash if changed
   * - attach callback for address change and store in flash
   * - load dmx address from flash and set in config on startup
   * - attach callback to rdm identify and flash leds when on
   * - Make all important config variables available via rdm
   * - RDM_PID_DEVICE_LABEL does not seem to be supported, yet? Implement in esp_dmx and create PR
   * - implement changing personality in rdm. (not yet implemented in esp_dmx?)
   *   - This is more complicated because get personality requests two bytes but
   *     set personality only contains one byte. Thus the default parameter callback will
   *     not work. Need to think about this :D
   */
  if (rxPin > 0 && enPin > 0 && txPin > 0)
  {

    const managed_pin_type pins[] = {
        {(int8_t)txPin, false}, // these are not used as gpio pins, thus isOutput is always false.
        {(int8_t)rxPin, false},
        {(int8_t)enPin, false}};
    const bool pinsAllocated = pinManager.allocateMultiplePins(pins, 3, PinOwner::DMX_INPUT);
    if (!pinsAllocated)
    {
      USER_PRINTF("DMXInput: Error: Failed to allocate pins for DMX_INPUT. Pins already in use:\n");
      USER_PRINTF("rx in use by: %s\n", pinManager.getPinOwnerText(rxPin).c_str());
      USER_PRINTF("tx in use by: %s\n", pinManager.getPinOwnerText(txPin).c_str());
      USER_PRINTF("en in use by: %s\n", pinManager.getPinOwnerText(enPin).c_str());
      return;
    }

    const auto config = createConfig();
    if (!dmx_driver_install(inputPortNum, &config, DMX_INTR_FLAGS_DEFAULT))
    {
      USER_PRINTF("Error: Failed to install dmx driver\n");
      return;
    }

    USER_PRINTF("Listening for DMX on pin %u\n", rxPin);
    USER_PRINTF("Sending DMX on pin %u\n", txPin);
    USER_PRINTF("DMX enable pin is: %u\n", enPin);
    dmx_set_pin(inputPortNum, txPin, rxPin, enPin);

    rdm_register_dmx_start_address(inputPortNum, rdmAddressChangedCb, this);
    initialized = true;
  }
  else
  {
    USER_PRINTLN("DMX input disabled due to rxPin, enPin or txPin not set");
    return;
  }
}

void DMXInput::update()
{
  if (!initialized)
  {
    return;
  }

  checkAndUpdateConfig();

  byte dmxdata[DMX_PACKET_SIZE];
  dmx_packet_t packet;
  unsigned long now = millis();
  if (dmx_receive(inputPortNum, &packet, 0))
  {
    if (!packet.err)
    {
      if (!connected)
      {
        USER_PRINTLN("DMX is connected!");
        connected = true;
      }

      if (isIdentifyOn())
      {
        DEBUG_PRINTLN("RDM Identify active");
        turnOnAllLeds();
      }
      else if (!packet.is_rdm)
      {
        dmx_read(inputPortNum, dmxdata, packet.size);
        handleDMXData(1, 512, dmxdata, REALTIME_MODE_DMX, 0);
      }

      lastUpdate = now;
    }
    else
    {
      /*This can happen when you first connect or disconnect your DMX devices.
        If you are consistently getting DMX errors, then something may have gone wrong. */
      DEBUG_PRINT("A DMX error occurred - ");
      DEBUG_PRINTLN(packet.err); // TODO translate err code to string for output
    }
  }
  else if (connected && (now - lastUpdate > 5000))
  {
    connected = false;
    USER_PRINTLN("DMX was disconnected.");
  }
}

void DMXInput::turnOnAllLeds()
{
  // TODO not sure if this is the correct way?
  const uint16_t numPixels = strip.getLengthTotal();
  for (uint16_t i = 0; i < numPixels; ++i)
  {
    strip.setPixelColor(i, 255, 255, 255, 255);
  }
  strip.setBrightness(255, true);
  strip.show();
}

void DMXInput::disable()
{
  if (initialized)
  {
    dmx_driver_disable(inputPortNum);
  }
}
void DMXInput::enable()
{
  if (initialized)
  {
    dmx_driver_enable(inputPortNum);
  }
}

bool DMXInput::isIdentifyOn() const
{

  uint8_t identify = 0;
  const bool gotIdentify = rdm_get_identify_device(inputPortNum, &identify);
  // gotIdentify should never be false because it is a default parameter in rdm
  // but just in case we check for it anyway
  return bool(identify) && gotIdentify;
}

void DMXInput::checkAndUpdateConfig()
{

  /**
   * The global configuration variables are modified by the web interface.
   * If they differ from the driver configuration, we have to update the driver
   * configuration.
   */

  const uint8_t currentPersonality = dmx_get_current_personality(inputPortNum);
  if (currentPersonality != DMXMode)
  {
    DEBUG_PRINTF("DMX personality has changed from %d to %d\n", currentPersonality, DMXMode);
    dmx_set_current_personality(inputPortNum, DMXMode);
  }

  const uint16_t currentAddr = dmx_get_start_address(inputPortNum);
  if (currentAddr != DMXAddress)
  {
    DEBUG_PRINTF("DMX address has changed from %d to %d\n", currentAddr, DMXAddress);
    dmx_set_start_address(inputPortNum, DMXAddress);
  }
}

#endif