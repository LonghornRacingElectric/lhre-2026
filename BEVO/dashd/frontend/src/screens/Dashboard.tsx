import React, { useEffect, useState } from 'react';
import ScreenOne from './ScreenOne';
import ScreenTwo from './ScreenTwo';

const Dashboard: React.FC = () => {
    const [screenIndex, setScreenIndex] = useState<number>(0);

    useEffect(() => {
        const handleKeyDown = (event: KeyboardEvent) => {
            if (event.key === 'Enter') {
                setScreenIndex(prev => (prev === 0 ? 1 : 0));
            }
        };

        window.addEventListener('keydown', handleKeyDown);

        // Cleanup
        return () => {
            window.removeEventListener('keydown', handleKeyDown);
        };
    }, []);

    return (
        <div style={{ width: '100vw', height: '100vh', overflow: 'hidden' }}>
            {screenIndex === 0 ? <ScreenOne /> : <ScreenTwo />}
        </div>
    );
};

export default Dashboard;
