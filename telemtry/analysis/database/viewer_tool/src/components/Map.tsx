'use client';

import { MapContainer, TileLayer, Marker, Popup, useMap } from 'react-leaflet';
import 'leaflet/dist/leaflet.css';
import L from 'leaflet';
import { useState, useEffect, useRef } from 'react';
import { nanoid } from 'nanoid';

// Fix for default icon issue with webpack
// eslint-disable-next-line @typescript-eslint/no-explicit-any
delete (L.Icon.Default.prototype as any)._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl: 'https://unpkg.com/leaflet@1.7.1/dist/images/marker-icon-2x.png',
  iconUrl: 'https://unpkg.com/leaflet@1.7.1/dist/images/marker-icon.png',
  shadowUrl: 'https://unpkg.com/leaflet@1.7.1/dist/images/marker-shadow.png',
});

const dotIcon = new L.DivIcon({
    className: 'custom-dot-marker',
    html: `<div style="background-color: blue; width: 12px; height: 12px; border-radius: 50%; border: 2px solid white;"></div>`,
    iconSize: [12, 12],
    iconAnchor: [6, 6]
  });

const MapUpdater = ({ position }: { position: [number, number] }) => {
  const map = useMap();
  useEffect(() => {
    map.setView(position, map.getZoom());
  }, [position, map]);
  return null;
}

const Map = () => {
  const [position, setPosition] = useState<[number, number]>([51.505, -0.09]);
  const [error, setError] = useState<string | null>(null);
  const watcherRef = useRef<number | null>(null);
  const mapId = useRef(nanoid());

  useEffect(() => {
    if (navigator.geolocation) {
      watcherRef.current = navigator.geolocation.watchPosition((pos) => {
        const { latitude, longitude } = pos.coords;
        setPosition([latitude, longitude]);
        setError(null);
      }, (err) => {
        setError(err.message);
      }, {
        enableHighAccuracy: true,
        timeout: 5000,
        maximumAge: 0
      });
    } else {
        setError("Geolocation is not supported by this browser.");
    }

    return () => {
        if(watcherRef.current) {
            navigator.geolocation.clearWatch(watcherRef.current);
        }
    }
  }, []);

  if (error) {
    return (
        <div style={{ height: '100%', width: '100%', display: 'flex', justifyContent: 'center', alignItems: 'center' }}>
            <p>{error}</p>
        </div>
    )
  }

  return (
    <MapContainer key={mapId.current} id={mapId.current} center={position} zoom={19} style={{ height: '100%', width: '100%' }}>
      <TileLayer
        url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
        attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
      />
      <Marker position={position} icon={dotIcon}>
        <Popup>
          Your current location.
        </Popup>
      </Marker>
      <MapUpdater position={position} />
    </MapContainer>
  );
};

export default Map;
