# Humidity Controller

Measures co2, RH, and temp of an environment, as well as a controllable PWM fan. The control plane is home assistant. 

Uses PlatformIO to manage dependencies

Needs a .env

## Tips

`mqtt pub -t humiditycontroller/fan -m "fan_on" -h localhost`