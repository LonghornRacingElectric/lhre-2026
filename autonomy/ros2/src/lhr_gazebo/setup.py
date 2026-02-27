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
            glob('models/fsae_vehicle/*')),
        (os.path.join('share', package_name, 'models', 'cone_blue'),
            glob('models/cone_blue/*')),
        (os.path.join('share', package_name, 'models', 'cone_yellow'),
            glob('models/cone_yellow/*')),
        (os.path.join('share', package_name, 'models', 'cone_orange_small'),
            glob('models/cone_orange_small/*')),
        (os.path.join('share', package_name, 'models', 'cone_orange_large'),
            glob('models/cone_orange_large/*')),
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
            'logical_camera_adapter = lhr_gazebo.logical_camera_adapter:main',
            'ackermann_cmd_adapter = lhr_gazebo.ackermann_cmd_adapter:main',
        ],
    },
)
