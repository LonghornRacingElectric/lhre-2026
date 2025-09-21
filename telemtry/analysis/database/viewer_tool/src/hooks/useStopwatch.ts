import { useState, useRef, useCallback } from 'react';

interface LapData {
  startTime: number;
  endTime: number;
  notes?: string;
}

const timeFormatter = (timeInMS: number) => {
  const time = new Date(timeInMS);
  const minutes = time.getMinutes().toString().padStart(2, '0');
  const seconds = time.getSeconds().toString().padStart(2, '0');
  const ms = time.getMilliseconds().toString().padStart(3, '0');
  return `${minutes}:${seconds}.${ms}`;
};

export const useStopwatch = () => {
  const [time, setTime] = useState(0);
  const [isRunning, setIsRunning] = useState(false);
  const intervalRef = useRef<NodeJS.Timeout | null>(null);
  const offsetRef = useRef(0);

  const [isTurning, setIsTurning] = useState(false);
  const [turnStartTime, setTurnStartTime] = useState(0);
  const [turns, setTurns] = useState<LapData[]>([]);

  const [isAccel, setIsAccel] = useState(false);
  const [accelStartTime, setAccelStartTime] = useState(0);
  const [accels, setAccels] = useState<LapData[]>([]);

  const [laps, setLaps] = useState<number[]>([]);

  const delta = useCallback(() => {
    const now = Date.now();
    const timePassed = now - offsetRef.current;
    offsetRef.current = now;
    return timePassed;
  }, []);

  const update = useCallback(() => {
    setTime((prevTime) => prevTime + delta());
  }, [delta]);

  const start = useCallback(() => {
    if (!isRunning) {
      setIsRunning(true);
      offsetRef.current = Date.now();
      intervalRef.current = setInterval(update, 10);
    }
  }, [isRunning, update]);

  const stop = useCallback(() => {
    if (isRunning) {
      setIsRunning(false);
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
        intervalRef.current = null;
      }
    }
  }, [isRunning]);

  const reset = useCallback(() => {
    setTime(0);
    setTurns([]);
    setAccels([]);
    setLaps([]);
  }, []);

  const toggleTurn = useCallback(() => {
    let newTurns = turns;
    if (isTurning) {
      setIsTurning(false);
      newTurns = [...turns, { startTime: turnStartTime, endTime: time }];
      setTurns(newTurns);
    } else {
      setIsTurning(true);
      setTurnStartTime(time);
    }
    return newTurns;
  }, [isTurning, time, turnStartTime, turns]);

  const toggleAccel = useCallback(() => {
    let newAccels = accels;
    if (isAccel) {
      setIsAccel(false);
      newAccels = [...accels, { startTime: accelStartTime, endTime: time }];
      setAccels(newAccels);
    } else {
      setIsAccel(true);
      setAccelStartTime(time);
    }
    return newAccels;
  }, [isAccel, time, accelStartTime, accels]);

  const addLap = useCallback(() => {
    const newLaps = [...laps, time];
    setLaps(newLaps);
    return newLaps;
  }, [laps, time]);

  return {
    time,
    formattedTime: timeFormatter(time),
    isRunning,
    start,
    stop,
    reset,
    isTurning,
    toggleTurn,
    turns,
    setTurns,
    isAccel,
    toggleAccel,
    accels,
    setAccels,
    laps,
    setLaps,
    addLap,
    timeFormatter,
  };
};