# Humidity Controller

Measures co2, RH, and temp of an environment, as well as a controllable PWM fan. 

The control plane is home assistant. 

Uses PlatformIO to manage dependencies

Needs a `.env`, see `dev.env`

## Tips

### MQTT

`mqtt pub -t humiditycontroller/fan -m "fan_on" -h localhost`

### Platform IO

`pio device list`

`pio project config --list-envs`

`pio device monitor -e <environment_name>`

`pio run -e esp32_feather_esp32s2`

`pio run -e <environment_name> --target upload`
