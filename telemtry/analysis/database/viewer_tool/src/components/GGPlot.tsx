import { useEffect, useState } from 'react';
import { useKafkaJSON } from '@/hooks/useKafkaStream';
import * as d3 from 'd3';

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
        { x: newPoint.data.x, y: newPoint.data.y },
      ]);
    }
  }, [newPoint]);

  const width = 500;
  const height = 500;

  // Calculate dynamic range based on actual data with amplification
  const amplificationFactor = 1000; // Amplify small differences
  const padding = 0.1; // Add some padding around the data
  
  let xMin = -0.001, xMax = 0.001, yMin = -0.001, yMax = 0.001;
  
  if (points.length > 0) {
    const xValues = points.map(p => p.x * amplificationFactor);
    const yValues = points.map(p => p.y * amplificationFactor);
    
    xMin = Math.min(...xValues) - padding;
    xMax = Math.max(...xValues) + padding;
    yMin = Math.min(...yValues) - padding;
    yMax = Math.max(...yValues) + padding;
    
    // Ensure minimum range for visibility
    const minRange = 0.1;
    if (xMax - xMin < minRange) {
      const center = (xMax + xMin) / 2;
      xMin = center - minRange / 2;
      xMax = center + minRange / 2;
    }
    if (yMax - yMin < minRange) {
      const center = (yMax + yMin) / 2;
      yMin = center - minRange / 2;
      yMax = center + minRange / 2;
    }
  }

  const graphConfig = {
    scale: {
      x: d3.scaleLinear().domain([xMin, xMax]).range([0, width]),
      y: d3.scaleLinear().domain([yMin, yMax]).range([height, 0]),
    },
  };

  return (
    <div className="bg-white rounded-lg shadow-md p-4 w-full h-full flex flex-col">
      <h2 className="text-lg font-bold mb-4">GG Plot</h2>

      <div className="flex-grow">
        {points.length > 0 ? (
          <svg viewBox={`0 0 ${width} ${height}`} className="w-full h-full">
            {points.map((point, index) => (
              <circle
                key={index}
                cx={graphConfig.scale.x(point.x * amplificationFactor)}
                cy={graphConfig.scale.y(point.y * amplificationFactor)}
                r="3"
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