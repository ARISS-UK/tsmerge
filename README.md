tsmerger - merge input from multiple DVB ground stations into a single output stream

Created by Philip Heron <phil@sanslogic.co.uk>

## Application Binaries

Dependencies: libjson-c-dev

```
apt install libjson-c-dev
make
```

### tsmerge

*tsmerge* accepts incoming TS Streams in UDP Packets on port 5678 as MPEG-TS.

It will then attempt to merge the TS Streams in realtime in order to give the best possible output.

The output is available on a TCP Socket on port 5679 as MPEG-TS.

For more information on internal operation see *TSMERGING.md*

### tspush

*tspush* accepts MPEG-TS from stdin, will filter invalid/padding packets, and upload the remainder to a *tsmerge* instance on a remote host over UDP.

This application accepts multiple arguments, see *./tspush -h*

### tsinfo

*tsinfo* accepts MPEG-TS from stdin, and on completion of the input, will print statistics on the MPEG-TS stream recognised. This is useful for determining the bitrate, validity, padding rate, and PCR rate, of MPEG-TS samples.

## Stats Monitor

*stats-monitor/monitor* is a node daemon that will serve a realtime tsmerge monitoring webpage on *http://localhost:5800/*. The daemon will listen to statistics and status output, by tsmerge on a local UDP port, and will feed these to the webpage in realtime via socketio.

To install npm dependencies: `cd stats-monitor && npm install`

To run: `stats-monitor/monitor`

## Additional Contributors

Phil Crump <phil@philcrump.co.uk>
