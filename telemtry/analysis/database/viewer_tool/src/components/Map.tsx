"use client";

import { MapContainer, TileLayer, Marker, Popup, useMap } from "react-leaflet";
import "leaflet/dist/leaflet.css";
import L from "leaflet";
import { useState, useEffect, useRef } from "react";
import { nanoid } from "nanoid";
import { useKafkaJSON } from "@/hooks/useKafkaStream";

// Fix for default icon issue with webpack
delete (L.Icon.Default.prototype as any)._getIconUrl;
L.Icon.Default.mergeOptions({
  iconRetinaUrl:
    "https://unpkg.com/leaflet@1.7.1/dist/images/marker-icon-2x.png",
  iconUrl: "https://unpkg.com/leaflet@1.7.1/dist/images/marker-icon.png",
  shadowUrl: "https://unpkg.com/leaflet@1.7.1/dist/images/marker-shadow.png",
});

const dotIcon = new L.DivIcon({
  className: "custom-dot-marker",
  html: `<div style="background-color: blue; width: 12px; height: 12px; border-radius: 50%; border: 2px solid white;"></div>`,
  iconSize: [12, 12],
  iconAnchor: [6, 6],
});

export type MapData = {
  dynamics?: { gps?: number[] | null };
};

const MapUpdater = ({ position }: { position: [number, number] }) => {
  const map = useMap();
  useEffect(() => {
    map.setView(position, map.getZoom());
  }, [position, map]);
  return null;
};

const MapResizer = ({ resize }: { resize: any }) => {
  const map = useMap();
  useEffect(() => {
    if (resize) {
      map.invalidateSize();
    }
  }, [resize, map]);
  return null;
};

const Map = ({ resize, data }: { resize?: any, data?: MapData | null; }) => {
  const [position, setPosition] = useState<[number, number]>([51.505, -0.09]);
  const [error, setError] = useState<string | null>(null);
  const mapId = useRef(nanoid());

  // Live connection to Kafka "sensor_data" topic
  const { data: liveData } = useKafkaJSON<MapData>({
    topic: "map",
    // Extend staleness so we keep last sample between slower updates
    staleAfterMs: 1000,
    merge: true,
    // No custom select: we want the whole object; default parser handles JSON
  });

  const sensorData = data !== undefined ? data : liveData;
  const latitude = sensorData?.dynamics?.gps?.[0];
  const longitude = sensorData?.dynamics?.gps?.[1];

  useEffect(() => {
    if (latitude != null && longitude != null) {
      const lat = Number(latitude);
      const lng = Number(longitude);
      if (!Number.isFinite(lat) || !Number.isFinite(lng)) {
        setError("No Current Car Position");
        return;
      }

      setPosition([lat, lng]);
      setError(null);
    } else {
      setError("No Current Car Position");
    }
  }, [latitude, longitude]);

  if (error) {
    return (
      <div
        style={{
          height: "100%",
          width: "100%",
          display: "flex",
          justifyContent: "center",
          alignItems: "center",
        }}
      >
        <p>{error}</p>
      </div>
    );
  }

  return (
    <MapContainer
      key={mapId.current}
      id={mapId.current}
      center={position}
      zoom={19}
      style={{ height: "100%", width: "100%" }}
    >
      <TileLayer
        url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
        attribution='&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
      />
      <Marker position={position} icon={dotIcon}>
        <Popup>Your current location.</Popup>
      </Marker>
      <MapUpdater position={position} />
      <MapResizer resize={resize} />
    </MapContainer>
  );
};

export default Map;
