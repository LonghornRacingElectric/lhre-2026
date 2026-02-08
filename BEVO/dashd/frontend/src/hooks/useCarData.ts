import { useCallback, useRef } from 'react';
import useWebSocket, { ReadyState } from 'react-use-websocket';
import { DashMessage } from '../types/DashData';

const WS_URL = 'ws://localhost:8001/';

interface UseCarDataResult {
    data: DashMessage | null;
    isConnected: boolean;
    lastSeq: number;
}

export function useCarData(enabled: boolean): UseCarDataResult {
    const lastMessage = useRef<DashMessage | null>(null);
    const lastSeq = useRef<number>(-1);

    const onMessage = useCallback((event: MessageEvent) => {
        try {
            const parsed: DashMessage = JSON.parse(event.data);
            lastMessage.current = parsed;
            lastSeq.current = parsed.seq;
        } catch {
            // Ignore malformed messages
        }
    }, []);

    const { readyState, lastJsonMessage } = useWebSocket(
        enabled ? WS_URL : null,
        {
            shouldReconnect: () => true,
            reconnectAttempts: Infinity,
            reconnectInterval: 1000,
            onMessage,
        }
    );

    // Use lastJsonMessage as fallback for reactivity (triggers re-render)
    const data: DashMessage | null = lastJsonMessage as DashMessage | null;

    return {
        data,
        isConnected: readyState === ReadyState.OPEN,
        lastSeq: data?.seq ?? -1,
    };
}
