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
    className?: string;
}

const VerticalGauge: React.FC<VerticalGaugeProps> = ({
    value,
    min = 0,
    max = 100,
    label = "",
    width = 60,
    height = 200,
    color = "#BF5700", // Burnt Orange
    className = ""
}) => {
    const clampedValue = Math.min(Math.max(value, min), max);
    const percentage = (clampedValue - min) / (max - min);
    
    const barHeight = height * percentage;
    const y = height - barHeight;

    const svgHeight = label ? height + 40 : height;

    return (
        <div className={`vertical-gauge-container ${className}`}>
            <svg width={width} height={svgHeight}>
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
                    className="fill-bar"
                />
                
                {/* Label & Value */}
                {/* We can keep these or hide them if the parent component handles layout. 
                    Given the new modern design relies on the parent for labels, 
                    we can keep them as fallbacks or hide them via CSS if needed. 
                    For now, I'll leave them but maybe the parent passes label="" to hide them.
                */}
                {label && (
                    <>
                        <text x={width/2} y={height + 20} className="vertical-gauge-value">
                            {Math.round(value)}
                        </text>
                        <text x={width/2} y={height + 35} className="vertical-gauge-label">
                            {label}
                        </text>
                    </>
                )}
            </svg>
        </div>
    );
};

export default VerticalGauge;