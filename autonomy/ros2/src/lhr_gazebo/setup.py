import os
from glob import glob

from setuptools import setup

package_name = 'lhr_gazebo'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'),
            glob('launch/*.launch.py')),
        (os.path.join('share', package_name, 'config'),
            glob('config/*')),
        (os.path.join('share', package_name, 'models', 'fsae_vehicle'),
            [f for f in glob('models/fsae_vehicle/*') if os.path.isfile(f)]),
        (os.path.join('share', package_name, 'models', 'fsae_vehicle', 'meshes'),
            glob('models/fsae_vehicle/meshes/*')),
        (os.path.join('share', package_name, 'models', 'cone_blue'),
            [f for f in glob('models/cone_blue/*') if os.path.isfile(f)]),
        (os.path.join('share', package_name, 'models', 'cone_blue', 'meshes'),
            glob('models/cone_blue/meshes/*')),
        (os.path.join('share', package_name, 'models', 'cone_yellow'),
            [f for f in glob('models/cone_yellow/*') if os.path.isfile(f)]),
        (os.path.join('share', package_name, 'models', 'cone_yellow', 'meshes'),
            glob('models/cone_yellow/meshes/*')),
        (os.path.join('share', package_name, 'models', 'cone_orange_small'),
            [f for f in glob('models/cone_orange_small/*') if os.path.isfile(f)]),
        (os.path.join('share', package_name, 'models', 'cone_orange_small', 'meshes'),
            glob('models/cone_orange_small/meshes/*')),
        (os.path.join('share', package_name, 'models', 'cone_orange_large'),
            [f for f in glob('models/cone_orange_large/*') if os.path.isfile(f)]),
        (os.path.join('share', package_name, 'models', 'cone_orange_large', 'meshes'),
            glob('models/cone_orange_large/meshes/*')),
        (os.path.join('share', package_name, 'worlds'),
            glob('worlds/*.sdf')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='gray',
    maintainer_email='gray@todo.todo',
    description='Gazebo simulation for FSAE driverless.',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'joint_cmd_adapter = lhr_gazebo.joint_cmd_adapter:main',
        ],
    },
)
