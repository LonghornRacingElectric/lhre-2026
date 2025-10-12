'use client';

import React, { useEffect } from 'react';
import { Button } from '@/components/ui/button';
import {
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableHeader,
  TableRow,
} from '@/components/ui/table';
import { useStopwatch } from '@/hooks/useStopwatch';
import Link from 'next/link';

const TimingDeltas = () => {
    const { formattedTime, start } = useStopwatch();

    // FAKE DATA: Replace with actual data from the backend
    const fakeLapData = [
        ['0:25.123', '0:30.456', '0:28.789'],
        ['0:24.987', '0:30.123', '0:28.543'],
        ['0:25.543', '0:30.789', '0:29.123'],
        ['0:24.789', '0:29.987', '0:28.321'],
    ];

    // TEMPORARY: This is a temporary variable for display purposes.
    // Replace with the actual delta value from the backend.
    const currentDelta = -0.13;

    useEffect(() => {
        start(Date.now(), 0);
    }, [start]);

  return (
    <div className="flex flex-col h-full">
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
        <div className="flex justify-between items-center mb-2">
            <div className="text-2xl font-mono text-gray-500">{formattedTime}</div>
            <div className={`text-4xl font-mono ${currentDelta >= 0 ? 'text-red-500' : 'text-green-500'}`}>
                {/* TODO: Wire up the actual delta value here */}
                {currentDelta.toFixed(2)}
            </div>
        </div>
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
    </div>
  );
};

export default TimingDeltas;
