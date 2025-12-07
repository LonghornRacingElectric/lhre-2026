import React, { useMemo } from 'react';
import './VerticalGauge.css';

interface VerticalGaugeProps {
    value: number;
    min?: number;
    max?: number;
    label?: string;
    width?: number;
    height?: number;
    color?: string;
}

const VerticalGauge: React.FC<VerticalGaugeProps> = ({
    value,
    min = 0,
    max = 100,
    label = "",
    width = 60,
    height = 200,
    color = "#BF5700" // Burnt Orange
}) => {
    const clampedValue = Math.min(Math.max(value, min), max);
    const percentage = (clampedValue - min) / (max - min);
    
    const barHeight = height * percentage;
    const y = height - barHeight;

    return (
        <div className="vertical-gauge-container">
            <svg width={width} height={height + 40}>
                {/* Background Bar */}
                <rect
                    x="0"
                    y="0"
                    width={width}
                    height={height}
                    fill="#333"
                    rx="5"
                />
                
                {/* Fill Bar */}
                <rect
                    x="0"
                    y={y}
                    width={width}
                    height={barHeight}
                    fill={color}
                    rx="5"
                    style={{ transition: "all 0.3s ease-in-out" }}
                />
                
                {/* Label & Value */}
                <text x={width/2} y={height + 20} className="vertical-gauge-value">
                    {Math.round(value)}
                </text>
                 <text x={width/2} y={height + 35} className="vertical-gauge-label">
                    {label}
                </text>
            </svg>
        </div>
    );
};

export default VerticalGauge;
