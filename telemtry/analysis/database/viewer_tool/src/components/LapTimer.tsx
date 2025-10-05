'use client';

import React, { useEffect, useRef } from 'react';
import { Button } from '@/components/ui/button';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table';
import { useStopwatch } from '@/hooks/useStopwatch';
import screenfull from 'screenfull';

import Link from 'next/link';

const LapTimer = () => {
    const { formattedTime, start } = useStopwatch();
    const ref = useRef<HTMLDivElement>(null);

    // Fake data for laps and sectors
    const fakeLapData = [
        ['0:25.123', '0:30.456', '0:28.789'],
        ['0:24.987', '0:30.123', '0:28.543'],
        ['0:25.543', '0:30.789', '0:29.123'],
        ['0:24.789', '0:29.987', '0:28.321'],
    ];

    useEffect(() => {
        start();
    }, [start]);

    const handleFullscreen = () => {
        if (screenfull.isEnabled && ref.current) {
            screenfull.toggle(ref.current);
        }
    };


  return (
    <Card ref={ref} className="bg-card text-card-foreground h-full flex flex-col">
        <style jsx>{`
            @keyframes pulse-size {
                50% {
                    transform: scale(1.2);
                }
            }
            .pulse-size {
                animation: pulse-size 1.5s infinite;
            }
        `}</style>
        <CardHeader className="flex flex-row items-center justify-between">
            <CardTitle>Lap Timer</CardTitle>
            <Button variant="ghost" size="icon" onClick={handleFullscreen}>
                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3"/></svg>
            </Button>
        </CardHeader>
        <CardContent className="flex-grow flex flex-col">
            <div className="text-4xl font-mono text-center mb-2">{formattedTime}</div>
            <p className="text-center text-xs text-gray-500 mb-2">(Fake Data)</p>
            <div className="flex-grow overflow-auto">
                <Table>
                    <TableHeader>
                    <TableRow>
                        <TableHead>Lap</TableHead>
                        <TableHead>Sector 1</TableHead>
                        <TableHead>Sector 2</TableHead>
                        <TableHead>Sector 3</TableHead>
                    </TableRow>
                    </TableHeader>
                    <TableBody>
                    {fakeLapData.map((lap, lapIndex) => (
                        <TableRow key={lapIndex}>
                        <TableCell>{lapIndex + 1}</TableCell>
                        {lap.map((sectorTime, sectorIndex) => (
                            <TableCell key={sectorIndex}>{sectorTime}</TableCell>
                        ))}
                        </TableRow>
                    ))}
                    </TableBody>
                </Table>
            </div>
            <div className="flex justify-end items-center mt-2">
                {/* 
                    Status Indicator for Track Setup:
                    - Red: Not yet fully set up.
                    - Yellow: Setup in progress (track is mapping or mapped but no sectors have been defined yet).
                    - Green: Track is mapped and at least 1 sector exists.
                    The setup page at /track-mapping should update the status of this indicator.
                    This is currently using fake data and is set to green.
                */}
                <div 
                    className="w-4 h-4 rounded-full bg-green-500 pulse-size mr-2"
                    title="Red: Not set up. Yellow: Setup in progress. Green: Ready."
                ></div>
                <Link href="/track-mapping">
                    <Button variant="outline">Setup & Track Mapping</Button>
                </Link>
            </div>
        </CardContent>
    </Card>
  );
};

export default LapTimer;
