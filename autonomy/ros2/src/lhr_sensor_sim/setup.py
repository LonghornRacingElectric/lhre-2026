from setuptools import setup

package_name = 'lhr_sensor_sim'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='gray',
    maintainer_email='gray@todo.todo',
    description='FOV-limited sensor simulation for FSAE driverless.',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'sensor_sim = lhr_sensor_sim.sensor_sim_node:main',
        ],
    },
)
