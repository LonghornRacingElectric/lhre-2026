import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from datetime import datetime


import os
from pathlib import Path

def convert_to_epoch(row):
     
    dt = datetime(
        year=int(row['Year']) + 2000,
        month=int(row['Month']),
        day=int(row['Day']),
        hour=int(row['Hour']) - 5,
        minute=int(row['Minute']),
        second=int(row['Seconds']),
        microsecond=int(row['Milliseconds']) * 1000
    )
    return int(dt.timestamp())

file_path = os.path.join(Path(__file__).parent, 'Log__2024_10_11__05_50_47.csv')

if not os.path.exists(file_path):
    raise FileNotFoundError(f"File {file_path} not found.")

df = pd.read_csv(file_path)

print("Data from Excel:")
print(df.head())

fig1, axs1 = plt.subplots(5, sharex=True)
[ax_.set_xlim(75, 175) for ax_ in axs1]

for ax, dir in zip(axs1, ['X', 'Y', 'Z']):
    ax.plot(df.Time, -df[f'PDU Acceleration {dir}'])
    if dir == 'X': ax.plot(df.Time, df['Actual Torque'] / 10, 'r')
    if dir == 'Y': ax.plot(df.Time, (df['Steer Voltage'] - 1.25) * -20, 'r')
    ax.grid()
    
axs1[3].plot(df.Time, df['Speed'], 'b')
axs1[3].grid()

# fig3, ax3 = plt.subplots(1, sharex=True)
# ax3.plot(df.Time, df['Actual Torque'])
# ax3.set_xlim(75, 175)

# fig4, ax4 = plt.subplots(1, sharex=True)
# ax4.plot(df.Time, df['Steer Voltage'], 'r')
# ax4.set_xlim(75, 175)



times = [convert_to_epoch(df.iloc[0]) * 1000]
events = [0]

times += [np.float64(1728625927680.0), np.float64(1728625927680.0), np.float64(1728625932315.0), np.float64(1728625932315.0), np.float64(1728625933580.0), np.float64(1728625933580.0), np.float64(1728625937300.0), np.float64(1728625937300.0), np.float64(1728625939705.0), np.float64(1728625939705.0), np.float64(1728625944217.0), np.float64(1728625944217.0), np.float64(1728625945283.0), np.float64(1728625945283.0), np.float64(1728625950216.0), np.float64(1728625950216.0), np.float64(1728625951684.0), np.float64(1728625951684.0), np.float64(1728625953311.0), np.float64(1728625953311.0), np.float64(1728625960835.0), np.float64(1728625960835.0), np.float64(1728625964617.0), np.float64(1728625964617.0), np.float64(1728625964997.0), np.float64(1728625964997.0), np.float64(1728625969716.0), np.float64(1728625969716.0), np.float64(1728625972263.0), np.float64(1728625972263.0), np.float64(1728625976012.0), np.float64(1728625976012.0), np.float64(1728625978937.0), np.float64(1728625978937.0), np.float64(1728625983407.0), np.float64(1728625983407.0), np.float64(1728625984247.0), np.float64(1728625984247.0), np.float64(1728625989243.0), np.float64(1728625989243.0), np.float64(1728625991352.0), np.float64(1728625991352.0), np.float64(1728625992814.0), np.float64(1728625992814.0), np.float64(1728625997217.0), np.float64(1728625997217.0), np.float64(1728625998707.0), np.float64(1728625998707.0)]
events += [0, 1, 1, 0, 0, 1, 1, 0, 0, -1, -1, 0, 0, -1, -1, 0, 0, -1, -1, 0, 0, -1, -1, 0, 0, -1, -1, 0, 0, 1, 1, 0, 0, -1, -1, 0, 0, -1, -1, 0, 0, -1, -1, 0, 0, 1, 1, 0]

for i in range(len(times) - 1):
    if times[i+1] < times[i]:
        print(f"PROBLEM HERE AT INDEX {i}")

times.insert(1, times[1])
events.insert(1, 0)
times.append(times[-1])
events.append(0)
times.append(convert_to_epoch(df.iloc[-1]) * 1000)
events.append(0)

print(df['Speed'].describe())

df_events = pd.DataFrame({'Time': times, 'Event': events})
df_events['Time'] = df['Time'][0] + df_events['Time'] / 1000 - convert_to_epoch(df.iloc[0])

print(convert_to_epoch(df.iloc[0]))
print(df_events['Time'][0])




ax2 = plt.figure().add_subplot(projection='3d')

color_mapping = {1: 'green', -1: 'red', 0: 'black'}
colors = [color_mapping[event] for event in df_events['Event']]
for i in range(len(df_events['Time']) - 1):
    start_index = df['Time'].where(df['Time'] >= df_events['Time'][i]).argmin()
    end_index = df['Time'].where(df['Time'] >= df_events['Time'][i + 1]).argmin()

    
    plt.plot(df['Latitude'].iloc[start_index:end_index], df['Longitude'].iloc[start_index:end_index], zs=df['Time'].iloc[start_index:end_index], color=colors[i])


# ax2.plot(df['Latitude'], df['Longitude'], zs=df['Time'])


axs1[4].plot(df_events['Time'], df_events['Event'], 'g')
axs1[4].grid()

plt.show()