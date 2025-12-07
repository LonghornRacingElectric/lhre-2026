import React, { useMemo, useEffect, useRef } from 'react';
import './RadialGauge.css';

interface RadialGaugeProps {
    value: number;
    min?: number;
    max?: number;
    label?: string;
    size?: number; // Conceptual size of the gauge itself (excluding glow area)
    strokeWidth?: number;
    numTicks?: number;
    color?: string; // Hex color or "gradient"
    className?: string;
    showValueText?: boolean; // New prop to control internal value display
}

const RadialGauge: React.FC<RadialGaugeProps> = ({
    value,
    min = 0,
    max = 100,
    label = "",
    size = 200,
    strokeWidth = 15,
    numTicks = 10,
    color = "gradient",
    className = "",
    showValueText = true // Default to true for backward compatibility
}) => {
    const { PI, cos, sin } = Math;
    
    // Add padding to the SVG canvas for glow effect
    const svgPadding = 30; // Increased padding to accommodate 10px glow + some margin
    const svgSize = size + 2 * svgPadding;

    // Center of the gauge within the new SVG canvas size
    const cx = svgSize / 2;
    const cy = svgSize / 2;
    
    // Radius of the gauge arc, relative to the conceptual 'size' prop
    const r = (size - strokeWidth) / 2;

    // Standard 270 degree gauge:
    const gaugeStart = 3 * PI / 4; 
    const gaugeEnd = 9 * PI / 4; 
    
    const x1 = cx + r * cos(gaugeStart);
    const y1 = cy + r * sin(gaugeStart);
    const x2 = cx + r * cos(gaugeEnd);
    const y2 = cy + r * sin(gaugeEnd);

    // SVG Arc
    const d = `M ${x1} ${y1} A ${r} ${r} 0 1 1 ${x2} ${y2}`;
    
    const totalAngle = gaugeEnd - gaugeStart;
    const circumference = useMemo(() => r * totalAngle, [r, totalAngle]);
    
    // Clamp value
    const clampedValue = Math.min(Math.max(value, min), max);
    const percentage = (clampedValue - min) / (max - min);
    
    const strokeDashoffset = useMemo(
        () => circumference - percentage * circumference,
        [percentage, circumference]
    );

    const progressRef = useRef<SVGPathElement>(null);

    useEffect(() => {
        if (progressRef.current) {
            progressRef.current.style.transition = "stroke-dashoffset .3s ease-in-out";
            progressRef.current.style.strokeDashoffset = `${strokeDashoffset}`;
        }
    }, [strokeDashoffset]);

    // Ticks
    const ticks = useMemo(() => {
        const tickLength = 10;
        // eslint-disable-next-line @typescript-eslint/no-unused-vars
        const tickOffset = r - 15;

        return Array.from({ length: numTicks + 1}).map((_, i) => {
            const angle = gaugeStart + (totalAngle / numTicks) * i;
            const xStart = cx + (r - tickLength) * cos(angle);
            const yStart = cy + (r - tickLength) * sin(angle);
            const xEnd = cx + r * cos(angle);
            const yEnd = cy + r * sin(angle);

            return (
                <line
                    key={i}
                    x1={xStart}
                    y1={yStart}
                    x2={xEnd}
                    y2={yEnd}
                    stroke="#fff"
                    strokeWidth={2}
                    className="gauge-tick"
                />
            );
        });
    }, [numTicks, gaugeStart, totalAngle, cx, cy, r]);

    return (
        <div className={`radial-gauge-container ${className}`}>
            <svg width={svgSize} height={svgSize} viewBox={`0 0 ${svgSize} ${svgSize}`}>
                {/* Background track */}
                <path
                    fill="none"
                    stroke="#222"
                    strokeWidth={strokeWidth}
                    d={d}
                    strokeLinecap="round"
                    className="gauge-bg"
                />
                {/* Progress track */}
                <path
                    ref={progressRef}
                    fill="none"
                    stroke={color === "gradient" ? "url(#grad)" : color}
                    strokeWidth={strokeWidth}
                    strokeDasharray={`${circumference} ${circumference}`}
                    strokeDashoffset={circumference}
                    d={d}
                    strokeLinecap="round"
                    className="gauge-progress"
                />
                
                {ticks}

                {/* Gradient definition if needed */}
                {color === "gradient" && (
                    <defs>
                        <linearGradient id="grad" x1="0%" y1="0%" x2="100%" y2="0%">
                            <stop offset="0%" stopColor="#00ff00" />
                            <stop offset="50%" stopColor="#ffff00" />
                            <stop offset="100%" stopColor="#ff0000" />
                        </linearGradient>
                    </defs>
                )}
                
                {showValueText && (
                    <text x={cx} y={cy} className="radial-gauge-value">
                        {Math.round(value)}
                    </text>
                )}
                {label && ( // Only show label if provided
                    <text x={cx} y={cy + 30} className="radial-gauge-label">
                        {label}
                    </text>
                )}
            </svg>
        </div>
    );
};

export default RadialGauge;