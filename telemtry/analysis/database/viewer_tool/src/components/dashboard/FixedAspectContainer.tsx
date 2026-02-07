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
    const [isFullscreen, setIsFullscreen] = useState(false);

    useEffect(() => {
        const updateScale = () => {
            if (containerRef.current) {
                const containerWidth = containerRef.current.clientWidth;
                const containerHeight = containerRef.current.clientHeight;

                // Check if we're in fullscreen mode
                const fullscreen = document.fullscreenElement !== null;
                setIsFullscreen(fullscreen);

                if (fullscreen) {
                    // In fullscreen, fit within both dimensions
                    const scaleX = containerWidth / width;
                    const scaleY = containerHeight / height;
                    setScale(Math.min(scaleX, scaleY));
                } else {
                    // In tile mode, scale based on width and let aspect-ratio handle height
                    const scaleX = containerWidth / width;
                    setScale(scaleX);
                }
            }
        };

        updateScale();

        const resizeObserver = new ResizeObserver(updateScale);
        if (containerRef.current) {
            resizeObserver.observe(containerRef.current);
        }

        // Listen for fullscreen changes
        document.addEventListener('fullscreenchange', updateScale);

        return () => {
            resizeObserver.disconnect();
            document.removeEventListener('fullscreenchange', updateScale);
        };
    }, [width, height]);

    return (
        <div
            ref={containerRef}
            className="w-full flex items-center justify-center overflow-hidden"
            style={{
                background: '#000',
                // In fullscreen, fill the space; in tile mode, use aspect ratio
                ...(isFullscreen
                    ? { height: '100%' }
                    : { aspectRatio: `${width} / ${height}` }
                )
            }}
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
