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
    mode?: 'standard' | 'bidirectional'; // Bidirectional fills from 0
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
    showValueText = true,
    mode = 'standard'
}) => {
    const { PI, cos, sin, abs } = Math;
    
    // Add padding to the SVG canvas for glow effect
    const svgPadding = 30; 
    const svgSize = size + 2 * svgPadding;

    const cx = svgSize / 2;
    const cy = svgSize / 2;
    const r = (size - strokeWidth) / 2;

    const gaugeStart = 3 * PI / 4; 
    const gaugeEnd = 9 * PI / 4; 
    
    const x1 = cx + r * cos(gaugeStart);
    const y1 = cy + r * sin(gaugeStart);
    const x2 = cx + r * cos(gaugeEnd);
    const y2 = cy + r * sin(gaugeEnd);

    const d = `M ${x1} ${y1} A ${r} ${r} 0 1 1 ${x2} ${y2}`;
    
    const totalAngle = gaugeEnd - gaugeStart;
    const circumference = useMemo(() => r * totalAngle, [r, totalAngle]);
    
    const clampedValue = Math.min(Math.max(value, min), max);
    const percentage = (clampedValue - min) / (max - min);
    
    // Bidirectional Logic
    const zeroPercentage = (0 - min) / (max - min);
    
    let strokeDasharray = `${circumference} ${circumference}`;
    let strokeDashoffset = circumference - percentage * circumference;
    let strokeColor = color === "gradient" ? "url(#grad)" : color;

    if (mode === 'bidirectional') {
        const startFillPct = Math.min(zeroPercentage, percentage);
        const fillLengthPct = abs(percentage - zeroPercentage);
        
        const startGap = circumference * startFillPct;
        const fillLength = circumference * fillLengthPct;
        const endGap = circumference - startGap - fillLength;

        // Pattern: 0 gap length remaining
        strokeDasharray = `0 ${startGap} ${fillLength} ${endGap}`;
        strokeDashoffset = 0; // Offset logic handled by array pattern

        // Dynamic Color
        if (value < 0) strokeColor = "#00FF66"; // Regen (Green)
        else strokeColor = "#BF5700"; // Power (Orange)
    }

    const progressRef = useRef<SVGPathElement>(null);

    useEffect(() => {
        if (progressRef.current) {
            // Animate dasharray for bidirectional, dashoffset for standard
            progressRef.current.style.transition = "stroke-dashoffset .3s ease-in-out, stroke-dasharray .3s ease-in-out, stroke .3s ease";
            progressRef.current.style.strokeDashoffset = `${strokeDashoffset}`;
            progressRef.current.style.strokeDasharray = strokeDasharray;
            if (mode === 'bidirectional') {
                 // For bidirectional, we update stroke color dynamically via ref to avoid re-render flicker? 
                 // Actually passing prop is fine, React handles it.
            }
        }
    }, [strokeDashoffset, strokeDasharray, mode]);

    // Ticks
    const ticks = useMemo(() => {
        const tickLength = 10;
        return Array.from({ length: numTicks + 1}).map((_, i) => {
            const angle = gaugeStart + (totalAngle / numTicks) * i;
            // ... standard tick calc ...
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

    // Zero Tick for Bidirectional
    const zeroTick = useMemo(() => {
        if (mode !== 'bidirectional') return null;
        
        const angle = gaugeStart + totalAngle * zeroPercentage;
        const tickLength = 20; // Longer tick
        const xStart = cx + (r - tickLength) * cos(angle);
        const yStart = cy + (r - tickLength) * sin(angle);
        const xEnd = cx + (r + 5) * cos(angle); // Extend outward slightly
        const yEnd = cy + (r + 5) * sin(angle);

        return (
            <line
                x1={xStart}
                y1={yStart}
                x2={xEnd}
                y2={yEnd}
                stroke="#fff"
                strokeWidth={4}
                className="gauge-zero-tick"
            />
        );
    }, [mode, gaugeStart, totalAngle, zeroPercentage, cx, cy, r]);

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
                    stroke={strokeColor}
                    strokeWidth={strokeWidth}
                    strokeDasharray={strokeDasharray}
                    strokeDashoffset={strokeDashoffset}
                    d={d}
                    strokeLinecap="butt"
                    className="gauge-progress"
                />
                
                {ticks}
                {zeroTick}

                {/* Gradient definition if needed */}
                {color === "gradient" && mode !== 'bidirectional' && (
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
                {label && ( 
                    <text x={cx} y={cy + 30} className="radial-gauge-label">
                        {label}
                    </text>
                )}
            </svg>
        </div>
    );
};

export default RadialGauge;