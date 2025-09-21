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

const Banner = () => {
  const [time, setTime] = useState("");
  const [date, setDate] = useState("");

  useEffect(() => {
    const timer = setInterval(() => {
      const now = new Date();
      setTime(now.toLocaleTimeString());
      setDate(now.toLocaleDateString('en-US', { month: 'long', day: 'numeric', year: 'numeric' }));
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  return (
    <div className="bg-gray-800 text-white w-full h-14 flex items-center justify-between px-4 fixed top-0 left-0 right-0 z-50">
      <div className="flex items-center">
        <DropdownMenu>
          <DropdownMenuTrigger asChild>
            <Button variant="ghost" size="icon">
              <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><line x1="3" y1="12" x2="21" y2="12"></line><line x1="3" y1="6" x2="21" y2="6"></line><line x1="3" y1="18" x2="21" y2="18"></line></svg>
            </Button>
          </DropdownMenuTrigger>
          <DropdownMenuContent>
            <DropdownMenuItem asChild><Link href="/">Home</Link></DropdownMenuItem>
            <DropdownMenuItem asChild><Link href="/driveday">Driveday Page</Link></DropdownMenuItem>
            <DropdownMenuItem asChild><Link href="/tune">Texas Tune</Link></DropdownMenuItem>
            <DropdownMenuItem asChild><Link href="/dashboards">Grafana</Link></DropdownMenuItem>
          </DropdownMenuContent>
        </DropdownMenu>
        <div className="w-8 h-8 bg-gray-500 mr-2 ml-2"></div>
        <span>Welcome, User</span>
      </div>
      <div className="flex items-center">
        <span className="mr-4">{date}</span>
        <span>{time}</span>
      </div>
    </div>
  );
};

export default Banner;