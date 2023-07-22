# randr-helpers
automate monitor configurations

TL;DR: To automatically set up some RANDR configuration whenever output `HDMI1` is connected to a monitor made by `XYZ` with serial number `1234`, and some other configuration when it is disconnected:

```
$ randr-watch randr-select \
    --case HDMI1:XYZ:1234 --then xrandr $ARGS '' \
    --default xrandr $OTHER_ARGS  
```

# randr-watch
`randr-watch cmd [args...]` runs `cmd [args...]` each time a monitor is connected or disconnected; e.g., `randr-watch date` will print out timestamps at each change event.

# randr-list
`randr-list` returns a `,`-separated list of outputs and what they are attached to. Each item in the list is formatted as `output:manufacturer:model_no:serial_no`, where the last three fields are empty if the output is not connected; e.g.:

```
$ randr-list
eDP1:IVO:1334:0,DP1:::,DP2:::,HDMI1:::,HDMI2:::,VIRTUAL1:::
$ randr-watch randr-list
[runs randr-list each time a monitor is (dis)connected]
```

The fields are pulled from the EDID data, so the manufacturer is exactly 3 capital letters, the model number is at most 65535, and the serial number may not be set.

# randr-select

`randr-select` runs `randr-list` to get a list of connected monitors then execs into another program depending on its arguments. The syntax is:

```
randr-select [--case CASE --then cmd [args...] '']... [--default cmd [args...] '']
```

`CASE` is a condition that needs to be met for its command to be run. The syntax is

```
SPEC [--and|--or SPEC]... 
```

`SPEC` looks like one of the specifications in `randr-list`, but it can be cut short, e.g., `DP1:XYZ` will match any device manufactured by `XYZ` connected to `DP1`. `DP1` means the same thing as `DP1:::`, i.e., `DP1` is disconnected. The first matching condition wins and `--default` is always matching, so put it last.
