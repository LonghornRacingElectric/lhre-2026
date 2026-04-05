import { useWebSocket, ReadyState } from 'react-use-websocket';
import { DashMessage } from '../types/DashData';

const WS_URL = 'ws://localhost:8001/';

interface UseCarDataResult {
    data: DashMessage | null;
    isConnected: boolean;
    lastSeq: number;
}

export function useCarData(enabled: boolean): UseCarDataResult {
    const { readyState, lastJsonMessage } = useWebSocket(
        enabled ? WS_URL : null,
        {
            shouldReconnect: () => true,
            reconnectAttempts: Infinity,
            reconnectInterval: 1000,
        }
    );

    const data: DashMessage | null = lastJsonMessage as DashMessage | null;

    return {
        data,
        isConnected: readyState === ReadyState.OPEN,
        lastSeq: data?.seq ?? -1,
    };
}
