# UltimateSensor V2 LD2460 Upgrade And Calibration

This guide applies only to UltimateSensor V2 hardware fitted with the optional
HLK-LD2460 module. The standard product ships with LD2412 + LD2450.

## Safe Installation

1. Disconnect USB-C, Ethernet and PoE power.
2. Remove the LD2450 and leave the LD2412 installed.
3. Install the LD2460 on its dedicated GPIO6/GPIO5 connection.
4. Verify 5V, GND, TX and RX against the PCB labels before restoring power.
5. Flash an UltimateSensor V2 firmware variant whose name ends in `LD2460`.

Never move, rotate or reconnect the module while the sensor is powered.

## Physical Orientation

The connector position alone does not define the radar direction. In the
official LD2460 side-mount coordinate system, the antenna end points forward
into the room as positive Y. Positive X points to the right when looking
forward from the radar.

Use two short walks to verify the module:

- walking straight away from the sensor should mainly increase Y;
- walking left-to-right should mainly change X from negative to positive.

If a sideways walk barely changes X, power the sensor off and correct the
physical orientation. A 90-degree rotation may be needed relative to an
incorrect initial installation, but clockwise or anticlockwise is not a
universal instruction. The firmware publishes the radar coordinates without
an automatic 90-degree rotation.

## Radar Configuration

UltimateSensor V2 LD2460 firmware requires `side` installation mode. It queries
the saved mode after every boot and changes it only when necessary. It also
reads and exposes:

- **Tracking Radar Installation Height**, in metres above the floor;
- **Tracking Radar Installation Angle**, in degrees.

Changing either value writes the documented command to the LD2460 and reads
both values back. The radar stores them across power cycles.

Enter the actual mounting values. Hi-Link recommends side mounting at `2.2-2.7
m` and `25-40 degrees`, with `2.6 m` and `30 degrees` as its example. Detection
can work outside this range, but coordinate accuracy can be reduced.

## Reflections And Duplicate Targets

Mirrors, metal wardrobes, windows and other large reflective surfaces can
produce a stable second target or displaced coordinates. Cover the reflective
surface temporarily when diagnosing this. Keep LD2412 and PIR enabled for
normal use; combined occupancy uses them as reliable presence fallbacks.

## References

- [SmartHomeShop LD2460 component](https://github.com/smarthomeshop/ld2460)
- [Hi-Link LD2460 user manual](https://revspace.nl/images/9/9d/HLK-LD2460_2T4R_Multi_Target_Trajectory_Tracking_Module_Manual_V1.1.pdf)
- [Hi-Link LD2460 UART protocol](https://revspace.nl/images/2/2c/HLK-LD2460%E4%B8%B2%E5%8F%A3%E5%8D%8F%E8%AE%AEV1.0_translated.pdf)
