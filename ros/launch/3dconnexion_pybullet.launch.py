"""Run the 3Dconnexion console with the PyBullet patient cart."""

from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from dvrk_simulator_base.launch import SystemProfile, simulator_launch


def generate_launch_description():
    try:
        directory = Path(get_package_share_directory("three_dconnexion")) / "dvrk_pybullet"
        if not directory.is_dir():
            raise FileNotFoundError(directory)
    except Exception:
        directory = Path(__file__).resolve().parent.parent.parent / "share" / "dvrk_pybullet"

    return simulator_launch("dvrk_pybullet", profile=SystemProfile(
        config=directory / "pybullet.yaml",
        cart_scene="ECM_PSM1_PSM2_PSM3.yaml",
        system_config=directory / "system-3Dconnexion-PyBullet.json",
        cwd=directory,
        preview=True,
    ))
