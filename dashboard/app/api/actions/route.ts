import { NextResponse } from 'next/server';

const REPO_OWNER = 'LonghornRacingElectric';
const REPO_NAME = 'lhre-2026';
const API_BASE = `https://api.github.com/repos/${REPO_OWNER}/${REPO_NAME}`;

export async function GET(request: Request) {
    const { searchParams } = new URL(request.url);
    const page = searchParams.get('page') || '1';
    const per_page = searchParams.get('per_page') || '50';

    try {
        const res = await fetch(`${API_BASE}/actions/runs?per_page=${per_page}&page=${page}`, {
            headers: {
                'Accept': 'application/vnd.github.v3+json',
                // Add Authorization header here if needed for private repos or rate limits
                // 'Authorization': `Bearer ${process.env.GITHUB_TOKEN}`
            },
            // Disable cache to ensure live data
            cache: 'no-store'
        });

        if (!res.ok) {
            return NextResponse.json({ error: 'Failed to fetch' }, { status: res.status });
        }

        const data = await res.json();
        return NextResponse.json(data);
    } catch (error) {
        return NextResponse.json({ error: 'Internal Server Error' }, { status: 500 });
    }
}
