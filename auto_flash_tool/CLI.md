python -m serial.tools.list_ports
列出所有PORT

esptool.py --chip esp32s3 --port COM8 erase_flash
重刷用

python -m esptool --chip esp32s3 --port COM8 --baud 460800 write_flash -z 0x0 "G:\設定新裝置用\firmware.factory.bin"
刷機用指令

設定用指令

meshtastic --set canned_message.inputbroker_pin_a 37
meshtastic --set canned_message.inputbroker_pin_b 26
meshtastic --set canned_message.inputbroker_pin_press 4
meshtastic --set canned_message.inputbroker_event_cw UP
meshtastic --set canned_message.inputbroker_event_ccw DOWN
meshtastic --set canned_message.inputbroker_event_press SELECT
meshtastic --set canned_message.enabled true
meshtastic --set canned_message.rotary1_enabled true
meshtastic --set canned_message.allow_input_source rotEnc1
meshtastic --set-canned-message "這是HermesXB0.2.8|你好|有人在嗎?"

Channels:
  Index 0: PRIMARY psk=secret { "psk": "isDhHrNpJPlGX3GBJBX6kjuK7KQNp4Z0M7OTDpnX5N4=", "name": "MeshTW", "id": 1, "uplinkEnabled": true, "downlinkEnabled": true, "moduleSettings": { "positionPrecision": 15, "isClientMuted": false }, "channelNum": 0 }
  Index 1: SECONDARY psk=secret { "psk": "y1HciVgpl5Hzh05KJUe/umWUH8XhG3UjR1rvZHfUHFU=", "name": "SignalTest", "uplinkEnabled": true, "downlinkEnabled": true, "moduleSettings": { "positionPrecision": 15, "isClientMuted": false }, "channelNum": 0, "id": 0 }
  Index 2: SECONDARY psk=secret { "psk": "y2jnf86fTpf/4AFAf+mCwbzRoxpCV0P90dqJo0+w/SY=", "name": "Emergency!", "uplinkEnabled": true, "moduleSettings": { "positionPrecision": 32, "isClientMuted": false }, "channelNum": 0, "id": 0, "downlinkEnabled": false }
  Index 3: SECONDARY psk=default { "psk": "AQ==", "uplinkEnabled": true, "moduleSettings": { "positionPrecision": 15, "isClientMuted": false }, "channelNum": 0, "name": "", "id": 0, "downlinkEnabled": false }

Primary channel URL: https://meshtastic.org/e/#CjcSIIrA4R6zaST5Rl9xgSQV-pI7iuykDaeGdDOzkw6Z1-TeGgZNZXNoVFclAQAAACgBMAE6AggPEhUIARAEGPoBIAsoBTgIQANIAVAbWAE
Complete URL (includes all channels): https://meshtastic.org/e/#CjcSIIrA4R6zaST5Rl9xgSQV-pI7iuykDaeGdDOzkw6Z1-TeGgZNZXNoVFclAQAAACgBMAE6AggPCjYSIMtR3IlYKZeR84dOSiVHv7pllB_F4Rt1I0da72R31BxVGgpTaWduYWxUZXN0KAEwAToCCA8KNBIgy2jnf86fTpf_4AFAf-mCwbzRoxpCV0P90dqJo0-w_SYaCkVtZXJnZW5jeSEoAToCCCAKCRIBASgBOgIIDxIVCAEQBBj6ASALKAU4CEADSAFQG1gB

 "mqtt": {
    "address": "mqtt.meshtastic.org",
    "username": "meshdev",
    "password": "large4cats",
    "encryptionEnabled": true,
    "root": "msh/TW",
    "enabled": false,
    "jsonEnabled": false,
    "tlsEnabled": false,
    "proxyToClientEnabled": false,
    "mapReportingEnabled": false

lora": {
    "usePreset": true,
    "modemPreset": "MEDIUM_FAST",
    "bandwidth": 250,
    "spreadFactor": 11,
    "codingRate": 5,
    "region": "TW",
    "hopLimit": 3,
    "txEnabled": true,
    "txPower": 27,
    "channelNum": 1,
    "frequencyOffset": 0.0,
    "overrideDutyCycle": false,
    "sx126xRxBoostedGain": false,
    "overrideFrequency": 0.0,
    "paFanDisabled": false,
    "ignoreIncoming": [],
    "ignoreMqtt": false,
    "configOkToMqtt": false

"position": {
    "positionBroadcastSecs": 60,
    "positionBroadcastSmartEnabled": true,
    "gpsUpdateInterval": 60,
    "positionFlags": 811,
    "broadcastSmartMinimumDistance": 100,
    "broadcastSmartMinimumIntervalSecs": 30,
    "gpsMode": "ENABLED",
    "fixedPosition": false,
    "gpsEnabled": false,
    "gpsAttemptTime": 0,
    "rxGpio": 0,
    "txGpio": 0,
    "gpsEnGpio": 0

📝 解釋
欄位名稱	說明
inputbroker_pin_a	Rotary A 相（通常是 CLK 腳）
inputbroker_pin_b	Rotary B 相（通常是 DT 腳）
inputbroker_pin_press	Rotary 按鈕
inputbroker_event_cw	
inputbroker_event_ccw	
inputbroker_event_press	
rotary1_enabled	是否啟用 Rotary Encoder 支援


python -m esptool --chip esp32s3 --port COM8 --baud 460800 write_flash -z 0x0 "G:\geek_guys_oldways\20251003_2.6.11.6-hermesX_b0.2\.pio\build\heltec-wireless-tracker\firmware.factory.bin"

