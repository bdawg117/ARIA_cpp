import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
import xacro


def generate_launch_description():
    pkg = get_package_share_directory('robot_description')

    # ── Process URDF ──
    xacro_file = os.path.join(pkg, 'urdf', 'robot.urdf.xacro')
    robot_description = xacro.process_file(xacro_file).toxml()

    world_file = os.path.join(pkg, 'worlds', 'wander_world.sdf')

    # ── Gz Sim ──
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                get_package_share_directory('ros_gz_sim'),
                'launch', 'gz_sim.launch.py'
            )
        ),
        launch_arguments={'gz_args': f'-r {world_file}'}.items()
    )

    # ── Robot State Publisher ──
    rsp = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        parameters=[{'robot_description': robot_description}]
    )

    # ── Spawn robot into Gz Sim ──
    spawn = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=[
            '-name', 'wander_bot',
            '-topic', 'robot_description',
            '-x', '0', '-y', '0', '-z', '0.15'
        ]
    )

    # ── Bridge: Gz topics <-> ROS 2 topics ──
    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=[
            '/scan@sensor_msgs/msg/LaserScan[gz.msgs.LaserScan',
            '/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist',
            '/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry',
            '/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock',
        ]
    )

    return LaunchDescription([
        gz_sim,
        rsp,
        spawn,
        bridge,
    ])
