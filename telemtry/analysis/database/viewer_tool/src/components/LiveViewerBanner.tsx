'use client';

import { useState, useEffect } from 'react';
import Image from 'next/image';
import Link from 'next/link';
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import { Button } from '@/components/ui/button';
import { signOut, useSession } from 'next-auth/react';
import { useKafkaJSON } from '@/hooks/useKafkaStream';


const LiveViewerBanner = () => {
  const { data: session } = useSession();
  const [time, setTime] = useState("");
  const [date, setDate] = useState("");

  // Live connection and status via Kafka "status" topic
  // Expecting messages like: { connected: boolean, battery?: number, odometer?: number }
  const { data: status, connected: sseConnected } = useKafkaJSON<{
    connected?: boolean;
    battery?: number;
    odometer?: number;
  }>({
    topic: 'status',
    // No custom select: we want the whole object; default parser handles JSON
  });

  const isConnected = (status?.connected ?? false) && sseConnected;


  useEffect(() => {
    const timer = setInterval(() => {
      const now = new Date();
      setTime(now.toLocaleTimeString());
      setDate(now.toLocaleDateString('en-US', { month: 'long', day: 'numeric', year: 'numeric' }));
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  const handleSignOut = async () => {
    await signOut({ redirect: false, callbackUrl: '/login' });
    window.location.href = '/login'; // Force a hard reload to re-evaluate middleware
  };

  return (
    <div className="bg-gray-800 text-white w-full min-h-14 h-auto md:h-14 flex flex-col md:flex-row items-center justify-between px-4 fixed top-0 left-0 right-0 z-[1000]">
      <div className="flex items-center justify-between w-full md:w-auto">
        <div className="flex items-center">
          <DropdownMenu>
            <DropdownMenuTrigger asChild>
              <Button variant="ghost" size="icon">
                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><line x1="3" y1="12" x2="21" y2="12"></line><line x1="3" y1="6" x2="21" y2="6"></line><line x1="3" y1="18" x2="21" y2="18"></line></svg>
              </Button>
            </DropdownMenuTrigger>
            <DropdownMenuContent className="z-[1001]">
              <DropdownMenuItem asChild><Link href="/">Home</Link></DropdownMenuItem>
              <DropdownMenuItem asChild><Link href="/driveday">Driveday Page</Link></DropdownMenuItem>
              <DropdownMenuItem asChild><Link href="/tune">Texas Tune</Link></DropdownMenuItem>
              <DropdownMenuItem asChild><Link href="/dashboards">Grafana</Link></DropdownMenuItem>
              <DropdownMenuItem asChild><Link href="/live-viewer">Live Viewer</Link></DropdownMenuItem>
              <DropdownMenuItem onClick={handleSignOut}>Logout</DropdownMenuItem>
            </DropdownMenuContent>
          </DropdownMenu>
          <Image src="/telem_logo.png" alt="Telemetry Logo" width={40} height={40} className="ml-2" />
          <span className="ml-2">Welcome, {session?.user?.name || session?.user?.username || 'Guest'}</span>
          <span className="ml-4">|</span>
          <span className="ml-4 font-bold">Live Viewer</span>
        </div>
      </div>
      <div className="flex items-center flex-wrap justify-center md:justify-end w-full md:w-auto">
        {/* Odometer Display */}
        <div className="flex items-center border border-gray-600 rounded-lg px-2 py-1 mr-2 text-sm">
          <span>{typeof status?.odometer === 'number' ? `${status.odometer.toFixed(2)} mi` : '— mi'}</span>
        </div>
        {/* Battery Percentage Display */}
        <div className="flex items-center border border-gray-600 rounded-lg px-2 py-1 mr-2 text-sm">
          {(() => {
            const batteryPercentage =
              typeof status?.battery === 'number' ? Math.max(0, Math.min(100, Math.round(status.battery))) : undefined;
            let batteryColor = 'gray';
            if (typeof batteryPercentage === 'number' && batteryPercentage > 50) {
              batteryColor = 'green';
            } else if (typeof batteryPercentage === 'number' && batteryPercentage > 20) {
              batteryColor = 'yellow';
            } else {
              batteryColor = 'red';
            }
            return (
              <>
                <svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg" className="mr-2">
                  <rect x="2" y="6" width="16" height="12" rx="1" stroke="currentColor" strokeWidth="2"/>
                  <path d="M20 9H22V15H20V9Z" fill="currentColor"/>
                  <rect x="4" y="8" width={typeof batteryPercentage === 'number' ? 14 * (batteryPercentage / 100) : 0} height="8" rx="0.5" fill={batteryColor}/>
                </svg>
                <span>{typeof batteryPercentage === 'number' ? `${batteryPercentage}%` : '— %'}</span>
              </>
            );
          })()}
        </div>
        <div className="flex items-center border border-gray-600 rounded-lg px-2 py-1 mr-2 text-sm">
          {/* TODO: Wire up the actual connection status here */}
          <div 
              className={`w-4 h-4 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'} mr-2 pulse-size`}
              title={isConnected ? 'Connected to car' : 'Not connected to car'}
          ></div>
          <span>Car Connection</span>
        </div>
        <div className="hidden md:flex items-center">
          <span className="mr-4">{date}</span>
          <span className="mr-4">{time}</span>
        </div>
        <Button 
          onClick={() => { localStorage.removeItem('featuresOrder'); window.location.reload(); }} 
          className="bg-red-500 hover:bg-red-600 text-sm px-2 py-1"
          title="Clear layout and other local data"
        >
          Clear
        </Button>
      </div>
    </div>
  );
};

export default LiveViewerBanner;