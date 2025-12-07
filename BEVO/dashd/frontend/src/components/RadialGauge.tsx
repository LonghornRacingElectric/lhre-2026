import React, { useMemo, useEffect, useRef } from 'react';
import './RadialGauge.css';

interface RadialGaugeProps {
    value: number;
    min?: number;
    max?: number;
    label?: string;
    size?: number;
    strokeWidth?: number;
    numTicks?: number;
    color?: string; // Hex color or "gradient"
}

const RadialGauge: React.FC<RadialGaugeProps> = ({
    value,
    min = 0,
    max = 100,
    label = "",
    size = 200,
    strokeWidth = 15,
    numTicks = 10,
    color = "gradient"
}) => {
    const { PI, cos, sin } = Math;
    const cx = size / 2;
    const cy = size / 2;
    const r = (size - strokeWidth) / 2;

    const startAngle = -PI * 0.75; // Starts at 135 degrees (bottom left)
    const endAngle = PI * -0.25;   // Ends at 45 degrees (bottom right) ?? 
    // Wait, let's look at the ref Speedometer.
    // ref: startAngle = -PI * 0.25 (which is -45deg??) 
    // ref: endAngle = PI + PI * 0.25 (225deg)
    // Actually let's just stick to a standard 270 degree gauge.
    // 135 deg to 405 deg (which is 45 deg)
    
    // Ref: startAngle = -PI * 0.25 (-45 deg? No, 0 is 3 o'clock). 
    // -PI/4 is top right? No, positive is clockwise usually in SVG? 
    // SVG coordinates: y increases downwards. 0 rad is 3 o'clock.
    // Clockwise is positive angle.
    // -PI/4 is Top Right.
    // Ref used startAngle = -PI * 0.25
    // EndAngle = PI + PI * 0.25 = 5PI/4 = 225 deg (Bottom Left).
    // So it went from Top Right (-45) to Bottom Left (225). That seems odd for a speedometer.
    
    // Let's implement a standard automotive gauge: 
    // Starts at 135 deg (Bottom Left) -> goes clockwise to 45 deg (Bottom Right)
    // 135 deg = 3PI/4. 
    // 45 deg (next cycle) = 2PI + PI/4 = 9PI/4.
    // Total sweep = 270 deg = 1.5 PI.
    
    // SVG 0 is 3 o'clock. 
    // 135 deg = 3PI/4 (Bottom Right? No, +y is down).
    // 0 = 3 o'clock
    // PI/2 = 6 o'clock
    // PI = 9 o'clock
    // 3PI/2 = 12 o'clock
    
    // We want Bottom Left (approx 7-8 o'clock) to Bottom Right (4-5 o'clock).
    // Let's say we want 135 deg (Bottom Right) to 405 deg (Bottom Right + 270).
    // Wait, let's just do:
    // Start: PI - PI/4 = 3PI/4 (135 deg - Bottom Right in SVG coord? No)
    // 0 (3), 90 (6), 180 (9), 270 (12).
    // We want start at ~ 135 deg (Bottom Right).
    // End at ~ 405 deg (Bottom Right).
    
    // Let's stick to what the ref did, but corrected for my understanding or just copy the logic if it looked good.
    // Ref logic:
    // startAngle = -PI * 0.25; // -45 deg (Top Right)
    // endAngle = PI + PI * 0.25; // 225 deg (Bottom Left)
    // This looks inverted or maybe I am visualizing it wrong.
    
    // Let's use a standard 270 degree gauge.
    // Start at 135 degrees (approx 7-8 o'clock on a watch face). 
    // In SVG radians (0 at 3 o'clock):
    // 3 o'clock = 0
    // 12 o'clock = -PI/2
    // 9 o'clock = -PI
    // 6 o'clock = PI/2
    
    // So 8 o'clock is approx -PI - PI/6?
    // Let's define it as: Start at -225 deg (-1.25 PI) -> End at -45 deg (-0.25 PI)? No that's top.
    // Start at 135 deg (3PI/4) (Bottom Right) is not what we want.
    
    // Let's just hardcode 135 deg to 45 deg (crossing 0/360).
    // Start Angle: 135 deg = 3 * PI / 4.
    // End Angle: 405 deg = 9 * PI / 4.
    
    const GAUGE_START_ANGLE = 0.75 * PI; // 135 deg (Bottom Right... wait, +y is down. 0 is Right. PI/2 is Down. PI is Left. 3PI/2 is Up.)
    // So 3PI/4 is Bottom Left. YES.
    const GAUGE_END_ANGLE = 2.25 * PI;   // 405 deg (Bottom Right).
    
    // Use these for calculation
    const gaugeStart = 3 * PI / 4; 
    const gaugeEnd = 9 * PI / 4; 
    
    const x1 = cx + r * cos(gaugeStart); // cx + ...
    const y1 = cy + r * sin(gaugeStart);
    const x2 = cx + r * cos(gaugeEnd);
    const y2 = cy + r * sin(gaugeEnd);

    // SVG Arc: A rx ry x-axis-rotation large-arc-flag sweep-flag x y
    // For 270 deg, large-arc-flag is 1.
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
        const tickOffset = r - 15; // move ticks inside

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
    }, [numTicks, gaugeStart, totalAngle, cx, cy, r]);
    
    // Digital Value
    // 

    return (
        <div className="radial-gauge-container">
            <svg width={size} height={size}>
                {/* Background track */}
                <path
                    fill="none"
                    stroke="#333"
                    strokeWidth={strokeWidth}
                    d={d}
                    strokeLinecap="round"
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
                
                <text x={cx} y={cy} className="radial-gauge-value">
                    {Math.round(value)}
                </text>
                <text x={cx} y={cy + 30} className="radial-gauge-label">
                    {label}
                </text>
            </svg>
        </div>
    );
};

export default RadialGauge;
