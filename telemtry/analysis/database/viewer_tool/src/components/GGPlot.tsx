import { useEffect, useState } from 'react';
import { useKafkaJSON } from '@/hooks/useKafkaStream';

const GGPlot = () => {
  const [points, setPoints] = useState<{ x: number; y: number }[]>([]);

  // Live connection to Kafka "gg_plot_data" topic
  const { data: newPoint, connected: kafkaConnected } = useKafkaJSON<{
    data: { x: number; y: number };
  }>({
    topic: 'gg-plot',
  });

  // Update points with new data using useEffect
  useEffect(() => {
    if (newPoint) {
      console.log('Received new point:', newPoint);
      setPoints((prevPoints) => [
        ...prevPoints,
        { x: newPoint.data.x * 100, y: newPoint.data.y * 100 },
      ]);
    }
  }, [newPoint]);

  return (
    <div className="bg-white rounded-lg shadow-md p-4 w-full h-full flex flex-col">
      <h2 className="text-lg font-bold mb-4">GG Plot</h2>

      <div className="flex-grow">
        {points.length > 0 ? (
          <svg viewBox="0 0 100 100" className="w-full h-full">
            {points.map((point, index) => (
              <circle
                key={index}
                cx={point.x}
                cy={point.y}
                r="1"
                fill="blue"
              />
            ))}
          </svg>
        ) : (
          <p className="text-gray-500">No data available</p>
        )}
      </div>
    </div>
  );
};

export default GGPlot;