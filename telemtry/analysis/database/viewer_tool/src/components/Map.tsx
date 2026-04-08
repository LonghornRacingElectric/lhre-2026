"use client";

import {
  MapContainer,
  TileLayer,
  Marker,
  Popup,
  Polyline,
  useMap,
} from "react-leaflet";
import "leaflet/dist/leaflet.css";
import L from "leaflet";
import { useState, useEffect, useRef } from "react";
import { nanoid } from "nanoid";
import { useKafkaJSON } from "@/hooks/useKafkaStream";
import { useCarSelection } from "@/lib/carSelection";
import { Button } from "@/components/ui/button";
import { Checkbox } from "@/components/ui/checkbox";
import { Label } from "@/components/ui/label";
import { Card } from "@/components/ui/card";

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

const TRAIL_MAX_POINTS = 1000;

const Map = ({ resize, data }: { resize?: any, data?: MapData | null; }) => {
  const { selectedCar, ssePath, matchesSelectedCar } = useCarSelection();
  const [position, setPosition] = useState<[number, number]>([51.505, -0.09]);
  const [trail, setTrail] = useState<Array<[number, number]>>([]);
  const [showTrail, setShowTrail] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const mapId = useRef(nanoid());

  // Live connection to Kafka "sensor_data" topic
  const { data: liveData } = useKafkaJSON<MapData>({
    topic: "map",
    car: selectedCar,
    ssePath,
    filter: matchesSelectedCar,
    // Extend staleness so we keep last sample between slower updates
    staleAfterMs: 1000,
    merge: true,
    // No custom select: we want the whole object; default parser handles JSON
  });

  const sensorData = data !== undefined ? data : liveData;
  const latitude = sensorData?.dynamics?.gps?.[0];
  const longitude = sensorData?.dynamics?.gps?.[1];

  useEffect(() => {
    setTrail([]);
  }, [selectedCar]);

  useEffect(() => {
    if (latitude != null && longitude != null) {
      const lat = Number(latitude);
      const lng = Number(longitude);
      if (!Number.isFinite(lat) || !Number.isFinite(lng)) {
        setError("No Current Car Position");
        return;
      }

      const nextPos: [number, number] = [lat, lng];
      setPosition(nextPos);
      setTrail((prev) => {
        const last = prev[prev.length - 1];
        if (last && last[0] === nextPos[0] && last[1] === nextPos[1]) {
          return prev;
        }

        const next = [...prev, nextPos];
        if (next.length <= TRAIL_MAX_POINTS) return next;
        return next.slice(next.length - TRAIL_MAX_POINTS);
      });
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
    <div className="relative h-full w-full">
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
        {showTrail && trail.length >= 2 ? (
          <Polyline
            positions={trail}
            pathOptions={{ color: "blue", weight: 2, opacity: 0.7 }}
          />
        ) : null}
        <Marker position={position} icon={dotIcon}>
          <Popup>Your current location.</Popup>
        </Marker>
        <MapUpdater position={position} />
        <MapResizer resize={resize} />
      </MapContainer>

      <div className="absolute right-2 top-2 z-[1000]">
        <Card className="gap-2 px-3 py-3">
          <div className="flex items-center gap-2">
            <Checkbox
              id="showTrail"
              checked={showTrail}
              onCheckedChange={(checked) => setShowTrail(checked === true)}
            />
            <Label htmlFor="showTrail">Show trail</Label>
          </div>
          <Button
            type="button"
            variant="outline"
            size="sm"
            onClick={() => setTrail([])}
            disabled={trail.length === 0}
          >
            Reset trail
          </Button>
        </Card>
      </div>
    </div>
  );
};

export default Map;
