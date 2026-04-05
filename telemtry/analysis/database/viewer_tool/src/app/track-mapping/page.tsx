"use client";

import React from 'react';
import TrackMapper from '@/components/TrackMapper';

const TrackMappingPage = () => {
  return (
    <div className="p-4">
      <h1 className="text-2xl font-bold mb-4">Track Mapping</h1>
      <div style={{ width: '100%', height: '70vh' }} className="border rounded">
        <TrackMapper />
      </div>
    </div>
  );
};

export default TrackMappingPage;
