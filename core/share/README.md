# saw3Dconnexion

`saw3Dconnexion` exposes a 3Dconnexion USB 6-DOF mouse as a cisst/SAW component.
Each component instance owns one HID device.  If a system needs two USB mice,
create two `mts3Dconnexion` components with different configuration files.

The configured interface, for example `MTMR`, provides:

- `measured_cp`: integrated virtual Cartesian pose in the user frame
- `measured_cv`: Cartesian velocity from the HID deflection reports in the user frame
- `local/measured_cp`, `local/measured_cv`, and `local/measured_cs`: same data in the local device base frame
- `base_frame` and `set_base_frame`: fixed transform from the local device base frame to the user frame
- `setpoint_cp`: current virtual pose, for compatibility
- `operating_state`, `state_command`, and `period_statistics`
- `gripper/measured_js` and `gripper/configuration_js` when enabled

Each configured button gets its own provided interface, for example `MTMR/left`,
with a `Button` event carrying `prmEventButton`.

The JSON configuration uses `name` for the provided interface used by dVRK's
`MTM_GENERIC` arm entry; it is not a list of devices.  Frames are derived from
that name: `name: "MTMR"` produces moving frame `MTMR` and reference frame
`MTMR_base` for `local/measured_*`.  The public `measured_*` commands apply
`base_frame`, which defaults to an identity transform into the `user` frame and
can be changed at runtime with `set_base_frame`; it is intentionally not part
of the JSON configuration.

`measured_cp` is a virtual pose integrated from `measured_cv`; it is not an
absolute tracked pose.  Adjust the `linear` and `angular` scale, `full_scale`,
and `deadband` fields in JSON for the specific 3Dconnexion model and the
intended dVRK mapping.  The generated configuration also supports optional
`axis_map` and `sign` fields under `linear` and `angular` for devices that need
axis reordering or inversion.

The gripper interface is enabled by default and is driven by the configured
`open_button` and `close_button`.  Set `gripper.enabled` to `false` to omit
`gripper/measured_js`.

## Qt example

After building and sourcing the workspace, launch the Qt example with an
explicit device configuration:

```sh
saw3DconnexionQtExample \
    -j install/saw3DconnexionCore/share/saw3Dconnexion/saw3Dconnexion-MTMR.json
```

The default MTMR sample matches the older SpaceNavigator USB id
`046d:c626`.  For newer devices using `256f:c635`, use
`saw3Dconnexion-SpaceMouseCompact-MTMR.json` or copy one of these files and
set `vendor_id`/`product_id` to the ids reported in the startup log.

If the device is listed by hidapi but fails to open on Linux, check access to
the matching `/dev/hidraw*` node.  A udev rule for development can match the
3Dconnexion vendor id, for example `ATTRS{idVendor}=="046d"` or
`ATTRS{idVendor}=="256f"`.

## Linux udev rules

On Linux, hidapi opens the `/dev/hidraw*` node directly.  If the component logs
that it found a matching device but failed to open the path, install a udev rule
to grant user access.

Create `/etc/udev/rules.d/99-3dconnexion.rules` with:

```udev
# 3Dconnexion SpaceNavigator, older Logitech/3Dconnexion USB id
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", ATTRS{idVendor}=="046d", ATTRS{idProduct}=="c626", MODE="0666"

# 3Dconnexion SpaceMouse Compact, newer 3Dconnexion USB id
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", ATTRS{idVendor}=="256f", ATTRS{idProduct}=="c635", MODE="0666"
```

Then reload udev and reconnect the device:

```sh
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Unplug and replug the 3Dconnexion device, then verify the relevant hidraw node
is accessible:

```sh
ls -l /dev/hidraw*
```

For a stricter multi-user setup, replace `MODE="0666"` with a group-based rule,
for example `GROUP="plugdev", MODE="0660"`, and add your user to that group.

## ROS

The ROS package is `three_dconnexion`.  It creates the `mts3Dconnexion`
component, attaches the Qt widget, and bridges all provided interfaces through
`cisst_ros_crtk`.  This includes the main interface, for example `MTMR`, and the
button event interfaces, for example `MTMR/left` and `MTMR/right`.

ROS 1:

```sh
rosrun three_dconnexion three_dconnexion \
    -j install/saw3DconnexionCore/share/saw3Dconnexion/saw3Dconnexion-MTMR.json
```

ROS 2:

```sh
ros2 run three_dconnexion three_dconnexion \
    -j install/saw3DconnexionCore/share/saw3Dconnexion/saw3Dconnexion-MTMR.json
```

The `Reset` button in the Qt widget sends the CRTK operating state command
`home`, which resets the virtual Cartesian pose through the component's
`state_command` command.
