'use client';

import React from 'react';

interface VerticalGaugeProps {
    value: number;
    min?: number;
    max?: number;
    label?: string;
    width?: number;
    height?: number;
    color?: string;
    className?: string;
}

const VerticalGauge: React.FC<VerticalGaugeProps> = ({
    value,
    min = 0,
    max = 100,
    label = "",
    width = 60,
    height = 200,
    color = "#BF5700",
    className = ""
}) => {
    const clampedValue = Math.min(Math.max(value, min), max);
    const percentage = (clampedValue - min) / (max - min);

    const barHeight = height * percentage;
    const y = height - barHeight;

    const svgHeight = label ? height + 40 : height;

    return (
        <div className="flex flex-col items-center justify-center">
            <svg width={width} height={svgHeight}>
                <rect
                    x="0"
                    y="0"
                    width={width}
                    height={height}
                    fill="#333"
                    rx="5"
                />

                <rect
                    x="0"
                    y={y}
                    width={width}
                    height={barHeight}
                    fill={color}
                    rx="5"
                    style={{ transition: "all 0.3s ease-in-out" }}
                />

                {label && (
                    <>
                        <text
                            x={width/2}
                            y={height + 20}
                            className="text-xl font-bold fill-white"
                            textAnchor="middle"
                        >
                            {Math.round(value)}
                        </text>
                        <text
                            x={width/2}
                            y={height + 35}
                            className="text-sm fill-gray-400"
                            textAnchor="middle"
                        >
                            {label}
                        </text>
                    </>
                )}
            </svg>
        </div>
    );
};

export default VerticalGauge;
