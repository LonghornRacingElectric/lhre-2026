
import { useState, useEffect, useCallback, useRef } from 'react';

const timeFormatter = (timeInMS: number) => {
  const time = new Date(timeInMS);
  const minutes = time.getMinutes().toString().padStart(2, '0');
  const seconds = time.getSeconds().toString().padStart(2, '0');
  const ms = time.getMilliseconds().toString().padStart(3, '0');
  return `${minutes}:${seconds}.${ms}`;
};

export const useStopwatch = (initialTime = 0) => {
  const [time, setTime] = useState(initialTime);
  const [isRunning, setIsRunning] = useState(false);
  const intervalRef = useRef<NodeJS.Timeout | null>(null);

  const start = useCallback((startTime: number, baseTime: number) => {
    setIsRunning(true);
    const update = () => {
      setTime(baseTime + (Date.now() - startTime));
    };
    update();
    intervalRef.current = setInterval(update, 10);
  }, []);

  const stop = useCallback((stopTime: number) => {
    setIsRunning(false);
    setTime(stopTime);
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
  }, []);

  return {
    time,
    formattedTime: timeFormatter(time),
    isRunning,
    start,
    stop,
    setTime,
    timeFormatter,
  };
};
