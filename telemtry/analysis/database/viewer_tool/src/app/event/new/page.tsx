
'use client';

import { useRouter } from 'next/navigation';
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Checkbox } from "@/components/ui/checkbox";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";

export default function NewEventPage() {
  const router = useRouter();

  const handleCreateEvent = () => {
    router.push('/event/tracker');
  };

  return (
    <div className="p-8 flex justify-center items-center">
      <Card className="w-full max-w-4xl">
        <CardHeader>
          <CardTitle>Create New Event</CardTitle>
          <CardDescription>Enter the details for the new event.</CardDescription>
        </CardHeader>
        <CardContent>
          <form>
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="driver_id">Choose a driver</Label>
                <Select><SelectTrigger><SelectValue placeholder="Select a driver" /></SelectTrigger><SelectContent>
                  <SelectItem value="0">Other</SelectItem>
                  <SelectItem value="1">Rylan Hanks</SelectItem>
                  <SelectItem value="2">Sohan Agnihotri</SelectItem>
                  <SelectItem value="3">Dylan Hammerback</SelectItem>
                  <SelectItem value="4">Andrew Cloran</SelectItem>
                  <SelectItem value="5">Ali Jensen</SelectItem>
                  <SelectItem value="6">David Easter</SelectItem>
                </SelectContent></Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="location_id">Choose a location</Label>
                <Select><SelectTrigger><SelectValue placeholder="Select a location" /></SelectTrigger><SelectContent>
                  <SelectItem value="0">Other</SelectItem>
                  <SelectItem value="1">Pickle - Innovation Blvd</SelectItem>
                  <SelectItem value="2">Pickle - Front Lot</SelectItem>
                  <SelectItem value="3">Pickle - Other Lot</SelectItem>
                  <SelectItem value="4">COTA - Lot J</SelectItem>
                  <SelectItem value="5">COTA - Lot H</SelectItem>
                  <SelectItem value="6">COTA - Go Kart Track</SelectItem>
                </SelectContent></Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="event_type">Choose an event type</Label>
                <Select><SelectTrigger><SelectValue placeholder="Select an event type" /></SelectTrigger><SelectContent>
                  <SelectItem value="0">Other</SelectItem>
                  <SelectItem value="1">Endurance</SelectItem>
                  <SelectItem value="2">Autocross</SelectItem>
                  <SelectItem value="3">Skidpad</SelectItem>
                  <SelectItem value="4">Straight Line Acceleration</SelectItem>
                  <SelectItem value="5">Straight Line Breaking</SelectItem>
                </SelectContent></Select>
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="car_id">Choose a car</Label>
                <Select><SelectTrigger><SelectValue placeholder="Select a car" /></SelectTrigger><SelectContent>
                  <SelectItem value="1">Easy Driver</SelectItem>
                  <SelectItem value="2">Lady Luck</SelectItem>
                </SelectContent></Select>
              </div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="car_weight">Car Weight</Label><Input id="car_weight" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="tow_angle">Tow Angle</Label><Input id="tow_angle" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="camber">Camber</Label><Input id="camber" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="ride_height">Ride Height</Label><Input id="ride_height" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="ackerman_adjustment">Ackerman Adjustment</Label><Input id="ackerman_adjustment" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="power_limit">Power Limit</Label><Input id="power_limit" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="shock_dampening">Shock Dampening</Label><Input id="shock_dampening" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="torque_limit">Torque Limit</Label><Input id="torque_limit" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="frw_pressure">Front Right Wheel Pressure</Label><Input id="frw_pressure" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="flw_pressure">Front Left Wheel Pressure</Label><Input id="flw_pressure" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="brw_pressure">Back Right Wheel Pressure</Label><Input id="brw_pressure" type="number" /></div>
              <div className="flex flex-col space-y-1.5"><Label htmlFor="blw_pressure">Back Left Wheel Pressure</Label><Input id="blw_pressure" type="number" /></div>
            </div>
            <div className="mt-6 flex space-x-4">
              <div className="flex items-center space-x-2"><Checkbox id="front_wing_on" /><Label htmlFor="front_wing_on">Front Wing On</Label></div>
              <div className="flex items-center space-x-2"><Checkbox id="rear_wing_on" /><Label htmlFor="rear_wing_on">Rear Wing On</Label></div>
              <div className="flex items-center space-x-2"><Checkbox id="regen_on" /><Label htmlFor="regen_on">Regen On</Label></div>
              <div className="flex items-center space-x-2"><Checkbox id="undertray_on" /><Label htmlFor="undertray_on">Undertray On</Label></div>
            </div>
            <div className="mt-6 flex justify-end">
              <Button type="button" onClick={handleCreateEvent}>Create Event</Button>
            </div>
          </form>
        </CardContent>
      </Card>
    </div>
  );
}
