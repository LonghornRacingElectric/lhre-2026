'use client';

import React, { useMemo, useEffect, useRef } from 'react';

interface RadialGaugeProps {
    value: number;
    min?: number;
    max?: number;
    label?: string;
    size?: number;
    strokeWidth?: number;
    numTicks?: number;
    color?: string;
    className?: string;
    showValueText?: boolean;
    mode?: 'standard' | 'bidirectional';
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

        strokeDasharray = `0 ${startGap} ${fillLength} ${endGap}`;
        strokeDashoffset = 0;

        if (value < 0) strokeColor = "#00FF66";
        else strokeColor = "#BF5700";
    }

    const progressRef = useRef<SVGPathElement>(null);

    useEffect(() => {
        if (progressRef.current) {
            progressRef.current.style.transition = "stroke-dashoffset .3s ease-in-out, stroke-dasharray .3s ease-in-out, stroke .3s ease";
            progressRef.current.style.strokeDashoffset = `${strokeDashoffset}`;
            progressRef.current.style.strokeDasharray = strokeDasharray;
        }
    }, [strokeDashoffset, strokeDasharray, mode]);

    const ticks = useMemo(() => {
        const tickLength = 10;
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
                />
            );
        });
    }, [numTicks, gaugeStart, totalAngle, cx, cy, r, cos, sin]);

    const zeroTick = useMemo(() => {
        if (mode !== 'bidirectional') return null;

        const angle = gaugeStart + totalAngle * zeroPercentage;
        const tickLength = 20;
        const xStart = cx + (r - tickLength) * cos(angle);
        const yStart = cy + (r - tickLength) * sin(angle);
        const xEnd = cx + (r + 5) * cos(angle);
        const yEnd = cy + (r + 5) * sin(angle);

        return (
            <line
                x1={xStart}
                y1={yStart}
                x2={xEnd}
                y2={yEnd}
                stroke="#fff"
                strokeWidth={4}
            />
        );
    }, [mode, gaugeStart, totalAngle, zeroPercentage, cx, cy, r, cos, sin]);

    const glowFilter = className.includes('glow-orange')
        ? 'drop-shadow(0 0 10px #BF5700)'
        : className.includes('glow-yellow')
        ? 'drop-shadow(0 0 10px #FFD700)'
        : className.includes('glow-gradient')
        ? 'drop-shadow(0 0 10px rgba(255, 200, 0, 0.6))'
        : 'none';

    return (
        <div className="flex flex-col items-center justify-center relative">
            <svg width={svgSize} height={svgSize} viewBox={`0 0 ${svgSize} ${svgSize}`}>
                <path
                    fill="none"
                    stroke="#222"
                    strokeWidth={strokeWidth}
                    d={d}
                    strokeLinecap="round"
                />
                <path
                    ref={progressRef}
                    fill="none"
                    stroke={strokeColor}
                    strokeWidth={strokeWidth}
                    strokeDasharray={strokeDasharray}
                    strokeDashoffset={strokeDashoffset}
                    d={d}
                    strokeLinecap="butt"
                    style={{ filter: glowFilter }}
                />

                {ticks}
                {zeroTick}

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
                    <text
                        x={cx}
                        y={cy}
                        className="text-3xl font-bold fill-white"
                        textAnchor="middle"
                        dominantBaseline="middle"
                    >
                        {Math.round(value)}
                    </text>
                )}
                {label && (
                    <text
                        x={cx}
                        y={cy + 30}
                        className="text-base fill-gray-400"
                        textAnchor="middle"
                        dominantBaseline="middle"
                    >
                        {label}
                    </text>
                )}
            </svg>
        </div>
    );
};

export default RadialGauge;
