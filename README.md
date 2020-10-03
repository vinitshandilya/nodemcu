# nodemcu
An Arduino based firmware for NodeMCU

Sample read/write commands:

1. {\"command\":\"write\",\"gpio\":\"2\",\"state\":\"LOW\"} - Write state to GPIO
2. {\"command\":\"read\",\"gpio\":\"2\"} - Read GPIO state
3. {"command":"blink","gpio":"2","interval":"100"} - Blink GPIO every x interval
4. {"command":"schd","gpio":"2","timeout":"10000"} - Toggle GPIO state after x milliseconds
