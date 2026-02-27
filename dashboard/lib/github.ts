const REPO_OWNER = 'LonghornRacingElectric';
const REPO_NAME = 'lhre-2026';
const API_BASE = `https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}`;

export interface FirmwareTarget {
    id: string;
    name: string;
    fullName: string;
    description: string;
    chip: string;
    bazelTarget: string;
}

export const FIRMWARE_TARGETS: FirmwareTarget[] = [
    { id: 'VCU', name: 'VCU', fullName: 'Vehicle Control Unit', description: 'Main vehicle controller managing torque vectoring, state machines, and safety systems.', chip: 'STM32G474', bazelTarget: '//VCU/firmware:release' },
    { id: 'HVC', name: 'HVC', fullName: 'High Voltage Controller', description: 'High-voltage firmware for cell monitoring, state-of-charge calculation, and safety shutdown.', chip: 'STM32G474', bazelTarget: '//HVC/firmware:release' },
    { id: 'CSM', name: 'CSM', fullName: 'Corner Sensor Module', description: 'Thermal management controller for radiator fans, coolant pumps, and temperature monitoring.', chip: 'STM32G474', bazelTarget: '//CSM/firmware:release' },
    { id: 'DUI', name: 'DUI', fullName: 'Driver User Interface', description: 'Dashboard display and driver controls interface with CAN bus communication.', chip: 'STM32G474', bazelTarget: '//DUI/firmware:release' },
    { id: 'LVBMS', name: 'LVBMS', fullName: 'Low Voltage BMS', description: 'Low-voltage battery management for the 12V system, monitoring and balancing.', chip: 'STM32G474', bazelTarget: '//LVBMS/firmware:release' },
    { id: 'TSM', name: 'TSM', fullName: 'Thermal Sensor Module', description: 'Safety-critical tractive system monitoring and shutdown sequencing.', chip: 'STM32G474', bazelTarget: '//TSM/firmware:release' },
    { id: 'USM', name: 'USM', fullName: 'Upright Sensor Module', description: 'Redundant safety module ensuring fail-safe vehicle shutdown capabilities.', chip: 'STM32G474', bazelTarget: '//USM/firmware:release' },
    { id: 'PDU', name: 'PDU', fullName: 'Power Distribution Unit', description: 'Low-voltage power distribution, fuse monitoring, and PWM control for accessories.', chip: 'STM32G474', bazelTarget: '//PDU/firmware:release' },
    { id: 'BEVO', name: 'BEVO', fullName: 'BEVO', description: 'Autonomous vehicle controller for the BEVO platform with CAN gateway.', chip: 'STM32G474', bazelTarget: '//BEVO:release' },
];

export interface Release {
    id: number;
    tag_name: string;
    name: string;
    body: string;
    html_url: string;
    created_at: string;
    published_at: string;
    draft: boolean;
    prerelease: boolean;
    author: {
        login: string;
        avatar_url: string;
    };
    assets: {
        name: string;
        browser_download_url: string;
        size: number;
    }[];
    targetPrefix?: string;
    version?: string;
}

export interface WorkflowRun {
    id: number;
    name: string;
    head_branch: string;
    head_sha: string;
    status: string;
    conclusion: string | null;
    html_url: string;
    created_at: string;
    updated_at: string;
    run_number: number;
    event: string;
}

export interface Commit {
    sha: string;
    commit: {
        message: string;
        author: {
            name: string;
            date: string;
        };
    };
    html_url: string;
    author: {
        login: string;
        avatar_url: string;
    } | null;
}

async function fetchWithCache(url: string): Promise<Response> {
    return fetch(url, {
        headers: {
            'Accept': 'application/vnd.github.v3+json',
        },
        next: { revalidate: 300 }, // cache for 5 minutes
    });
}

export function parseTagName(tagName: string): { prefix: string; version: string } {
    const lastSlash = tagName.lastIndexOf('/');
    if (lastSlash === -1) {
        return { prefix: '', version: tagName };
    }
    return {
        prefix: tagName.substring(0, lastSlash),
        version: tagName.substring(lastSlash + 1),
    };
}

export async function fetchReleases(): Promise<Release[]> {
    const res = await fetchWithCache(`${API_BASE}/releases?per_page=100`);
    if (!res.ok) return [];
    const releases: Release[] = await res.json();
    return releases.map(r => {
        const parsed = parseTagName(r.tag_name);
        return { ...r, targetPrefix: parsed.prefix, version: parsed.version };
    });
}

export async function fetchTags(): Promise<{ name: string; commit: { sha: string } }[]> {
    const res = await fetchWithCache(`${API_BASE}/tags?per_page=100`);
    if (!res.ok) return [];
    return res.json();
}

export async function fetchWorkflowRuns(page = 1): Promise<{ total_count: number; workflow_runs: WorkflowRun[] }> {
    const res = await fetchWithCache(`${API_BASE}/actions/runs?per_page=30&page=${page}`);
    if (!res.ok) return { total_count: 0, workflow_runs: [] };
    return res.json();
}

export async function fetchCommits(days = 7): Promise<Commit[]> {
    const since = new Date();
    since.setDate(since.getDate() - days);
    const res = await fetchWithCache(`${API_BASE}/commits?per_page=100&since=${since.toISOString()}`);
    if (!res.ok) return [];
    return res.json();
}

export function getTargetReleases(releases: Release[], targetId: string): Release[] {
    return releases.filter(r => {
        const prefix = r.targetPrefix || '';
        return prefix.startsWith(targetId + '/') || prefix === targetId;
    });
}

export function getGlobalReleases(releases: Release[]): Release[] {
    return releases.filter(r => !r.targetPrefix || r.targetPrefix === '');
}

export function getLatestRelease(releases: Release[]): Release | undefined {
    return releases.sort((a, b) => new Date(b.published_at).getTime() - new Date(a.published_at).getTime())[0];
}

export function groupCommitsByDay(commits: Commit[], days = 7): { day: string; count: number }[] {
    const dayNames = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
    const result: { day: string; count: number }[] = [];

    for (let i = days - 1; i >= 0; i--) {
        const date = new Date();
        date.setDate(date.getDate() - i);
        const dayKey = date.toISOString().split('T')[0];
        const dayName = dayNames[date.getDay()];
        const count = commits.filter(c => c.commit.author.date.startsWith(dayKey)).length;
        result.push({ day: dayName, count });
    }

    return result;
}
