# 3Dconnexion SpaceMouse with Newton Patient Cart

This setup uses a single 3Dconnexion SpaceMouse (`saw3Dconnexion`) to teleoperate
all three patient-side manipulators (PSM1, PSM2, PSM3) and the endoscope camera manipulator (ECM)
running in the Newton physics simulator.

Newton supplies the ECM and PSM1–PSM3 through ROS topics via `dvrk_arm_from_ros`.
The dVRK console pairs the single 3D mouse (`MTMR`) with all three PSMs and the ECM:
- PSM1 teleoperation (`MTMR` -> `PSM1`)
- PSM2 teleoperation (`MTMR` -> `PSM2`)
- PSM3 teleoperation (`MTMR` -> `PSM3`)
- ECM single-hand teleoperation (`MTMR` -> `ECM`, `TELEOP_ECM_ONE_HAND`)

## Running the Simulation

From the workspace root:

```bash
# Optional: if building the packages
colcon build --symlink-install --packages-select three_dconnexion three_dconnexion_config dvrk_newton dvrk_simulator_base

# Source the workspace
source install/setup.bash

# Run the launch file
ros2 launch three_dconnexion 3dconnexion_newton.launch.py
```

Arguments supported by the launch file:
- `headless`: Run without viewer GUI (`true` or `false`, default: `true` in launch, can be passed `headless:=false` to open the Newton viewer window)
- `exercise`: Scene / exercise YAML (default: `tray_cubes.yaml`)
- `rqt`: Start the dVRK monitor rqt plugin (`false` by default)

## Controlling Arms

Use the dVRK control panel to:
1. Power on and home the system.
2. Select which arm teleoperation is active (PSM1, PSM2, PSM3, or ECM).
3. Use the 3D mouse to control the selected arm.
