
'use client';

import { Line } from 'react-chartjs-2';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
} from 'chart.js';
import { useState } from 'react';

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend
);

const AdjustableChart = ({ title }: { title: string }) => {
  const [data, setData] = useState({
    labels: ["0.00", "1.00", "2.00", "3.00", "4.00", "5.00", "6.00", "7.00", "8.00", "9.00", "10.00"],
    datasets: [
      {
        label: 'Data 1',
        data: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
        borderColor: '#BF5700',
        backgroundColor: 'rgba(191, 87, 0, 0.5)',
      },
    ],
  });

  const options = {
    responsive: true,
    plugins: {
      legend: {
        display: false,
      },
      title: {
        display: true,
        text: title,
      },
    },
  };

  return (
    <div className="bg-gray-200 p-4 rounded-lg shadow-inner">
      <Line options={options} data={data} />
      <div className="mt-4">
        <table className="w-full table-auto">
          <thead>
            <tr>
              <th className="px-4 py-2">X</th>
              <th className="px-4 py-2">Y</th>
            </tr>
          </thead>
          <tbody>
            {data.labels.map((label, i) => (
              <tr key={i}>
                <td>{label}</td>
                <td>{data.datasets[0].data[i]}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default AdjustableChart;
