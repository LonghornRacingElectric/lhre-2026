'use client';

import React, { useRef, useState, useEffect } from 'react';

interface FixedAspectContainerProps {
    children: React.ReactNode;
    width?: number;
    height?: number;
}

const FixedAspectContainer: React.FC<FixedAspectContainerProps> = ({
    children,
    width = 800,
    height = 480
}) => {
    const containerRef = useRef<HTMLDivElement>(null);
    const [scale, setScale] = useState(1);

    useEffect(() => {
        const updateScale = () => {
            if (containerRef.current) {
                const containerWidth = containerRef.current.clientWidth;
                const containerHeight = containerRef.current.clientHeight;

                const scaleX = containerWidth / width;
                const scaleY = containerHeight / height;
                const newScale = Math.min(scaleX, scaleY);

                setScale(newScale);
            }
        };

        updateScale();

        const resizeObserver = new ResizeObserver(updateScale);
        if (containerRef.current) {
            resizeObserver.observe(containerRef.current);
        }

        return () => resizeObserver.disconnect();
    }, [width, height]);

    return (
        <div
            ref={containerRef}
            className="w-full h-full flex items-center justify-center overflow-hidden"
            style={{ background: '#000' }}
        >
            <div
                style={{
                    width: `${width}px`,
                    height: `${height}px`,
                    transform: `scale(${scale})`,
                    transformOrigin: 'center center',
                    flexShrink: 0
                }}
            >
                {children}
            </div>
        </div>
    );
};

export default FixedAspectContainer;
