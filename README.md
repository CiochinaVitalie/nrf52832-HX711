# nrf52832
ble_eighty

You need to just use a bit of math, so it will depend on how good your inner child is at math.
You use the standard formula for slope and intersection: y = mx + b ... or m = (y - b)/x
Here

y is the actual weight in whatever units you want (g, kg, oz, etc)
x is the raw value from the HX711 - from scale.read_average()
m is your slope (multiplier)
b is your intersection (offset) - also from scale.read_average() but with no weight, or using scale.tare()
So say you have a raw value of 10000 for 0 weight (tare) and 20000 for 1000g, and want readings in g
First, your offset (b) is 10000
To calculate your multiplier (m) just substitute into the formula
1000 = m * 20000 + 10000 ... or m = (1000 - 10000) / 20000
Thus m = -0.45

Your numbers will be completely different, but the method is the same.
You then put these values into your sketch via scale.set_scale(m) and scale.set_offset(b)
Even better if you don't hard-code them but allow them to be calculated/updated on demand, as they may change over time due to various reasons.
The example sketch that comes with the library partially shows this process.
