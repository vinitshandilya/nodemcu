# nodemcu
An Arduino based firmware for NodeMCU

Sample read/write commands:

1. {\"command\":\"write\",\"gpio\":\"2\",\"state\":\"LOW\"} - Write state to GPIO
2. {\"command\":\"read\",\"gpio\":\"2\"} - Read GPIO state
3. {"command":"blink","gpio":"2","interval":"100"} - Blink GPIO every x interval
4. {"command":"schd","gpio":"2","timeout":"10000"} - Toggle GPIO state after x milliseconds
5. {"command":"pwm","gpio":"2","duty_c":"240"} - Enable PWM output on GPIO with 0 < duty_c < 255 (higher values means lower duty)

The NodeMCU will receive commands on "intopic" and publish the command output on "outtopic". So, if you're using external MQTT client to send commands, please send above cmds to "intopic". Additionally, subscribe to "outtopic" to read results from NodeMCU. Use blank MQTT broker username and password.

<img width="584" alt="image" src="https://user-images.githubusercontent.com/7278088/161390167-0d5ab869-b7ef-40d2-8496-5ab11a5d58de.png">
