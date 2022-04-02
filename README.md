# nodemcu
An Arduino based firmware for NodeMCU

Sample read/write commands:

1. {\"command\":\"write\",\"gpio\":\"2\",\"state\":\"LOW\"} - Write state to GPIO
2. {\"command\":\"read\",\"gpio\":\"2\"} - Read GPIO state
3. {"command":"blink","gpio":"2","interval":"100"} - Blink GPIO every x interval
4. {"command":"schd","gpio":"2","timeout":"10000"} - Toggle GPIO state after x milliseconds

The NodeMCU will receive commands on "intopic" and publish the command output on "outtopic". So, if you're using external MQTT client to send commands, please send above cmds to "intopic". Additionally, subscribe to "outtopic" to read results from NodeMCU.

<img width="405" alt="image" src="https://user-images.githubusercontent.com/7278088/161386426-076ad003-a15d-46f5-86bd-ecd43ff82d8c.png">
