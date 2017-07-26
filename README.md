#Battery

A simple sysctl(3) program for OpenBSD that returns AC and Battery status that
is useful when wanting to display this information in a status bar such as in 
tmux(1) or wmii(1).

Examples of output include:
```
AC0: OFF BAT0: 55.81% (battery discharging)
```

or when given the verbose option:
```
AC0: Disconnected
Battery0: battery discharging
Battery0: 55.38% remaining
Battery0: design capacity of 68.04 Wh
Battery0: rate of 11.63 W
Battery0: Voltage is 10.81 VDC out of 10.80 VDC
Battery0: last charged to 68.04 Wh
Battery0: low capacity is set for 2.72 Wh
Battery0: warning capacity is set for 6.12 Wh
Battery0: remaining capacity at 37.68 Wh
```

```
usage: battery [OPTIONS]
Simple utility for printing battery status
        -h              this help message
        -v              verbose

```

##License
OpenBSD ISC
